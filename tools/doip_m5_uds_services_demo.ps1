param(
    [string]$Destination = "127.0.0.1",
    [ValidateRange(1, 65535)]
    [int]$Port = 13400,
    [ValidateRange(1, 30)]
    [int]$TimeoutSeconds = 5
)

$ErrorActionPreference = "Stop"

function Read-ExactBytes {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [int]$Length
    )

    $bytes = New-Object byte[] $Length
    $offset = 0
    while ($offset -lt $Length) {
        $read = $Stream.Read($bytes, $offset, $Length - $offset)
        if ($read -eq 0) {
            throw "Server closed the connection before sending a DoIP response."
        }
        $offset += $read
    }
    return ,$bytes
}

function Read-DoipFrame {
    param([System.Net.Sockets.NetworkStream]$Stream)

    $header = Read-ExactBytes -Stream $Stream -Length 8
    if ($header[0] -ne 0x02 -or $header[1] -ne 0xfd) {
        throw "Received a DoIP response with an invalid protocol-version preamble."
    }
    $payloadType = ([int]$header[2] -shl 8) -bor [int]$header[3]
    $payloadLength =
        ([int]$header[4] -shl 24) -bor
        ([int]$header[5] -shl 16) -bor
        ([int]$header[6] -shl 8) -bor
        [int]$header[7]
    if ($payloadLength -gt 16777216) {
        throw "DoIP response payload exceeds the configured client limit."
    }
    return [pscustomobject]@{
        PayloadType = $payloadType
        Payload = Read-ExactBytes -Stream $Stream -Length $payloadLength
    }
}

function Send-DoipFrame {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [int]$PayloadType,
        [byte[]]$Payload
    )

    $payloadLength = $Payload.Length
    $frame = [byte[]]@(
        0x02, 0xfd,
        [byte](($PayloadType -shr 8) -band 0xff),
        [byte]($PayloadType -band 0xff),
        [byte](($payloadLength -shr 24) -band 0xff),
        [byte](($payloadLength -shr 16) -band 0xff),
        [byte](($payloadLength -shr 8) -band 0xff),
        [byte]($payloadLength -band 0xff)
    ) + $Payload
    $Stream.Write($frame, 0, $frame.Length)
    $Stream.Flush()
}

function Send-DiagnosticRequest {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [int]$TesterAddress,
        [int]$EntityAddress,
        [byte[]]$UdsRequest,
        [byte[]]$ExpectedUdsResponse,
        [string]$Label
    )

    $diagnosticPayload = [byte[]]@(
        [byte](($TesterAddress -shr 8) -band 0xff),
        [byte]($TesterAddress -band 0xff),
        [byte](($EntityAddress -shr 8) -band 0xff),
        [byte]($EntityAddress -band 0xff)
    ) + $UdsRequest
    Send-DoipFrame -Stream $Stream -PayloadType 0x8001 -Payload $diagnosticPayload

    $acknowledgement = Read-DoipFrame -Stream $Stream
    if ($acknowledgement.PayloadType -ne 0x8002 -or
        $acknowledgement.Payload.Length -ne 5) {
        throw "Received an invalid Diagnostic Message acknowledgement for $Label."
    }
    $ackSourceAddress =
        ([int]$acknowledgement.Payload[0] -shl 8) -bor [int]$acknowledgement.Payload[1]
    $ackTargetAddress =
        ([int]$acknowledgement.Payload[2] -shl 8) -bor [int]$acknowledgement.Payload[3]
    if ($ackSourceAddress -ne $EntityAddress -or
        $ackTargetAddress -ne $TesterAddress -or
        $acknowledgement.Payload[4] -ne 0x00) {
        throw "Diagnostic Message acknowledgement had unexpected addresses or code for $Label."
    }
    Write-Output ("DoIP ACK for {0}: source=0x{1:X4} target=0x{2:X4}" -f
        $Label, $ackSourceAddress, $ackTargetAddress)

    if ($null -eq $ExpectedUdsResponse) {
        Write-Output ("UDS positive response suppressed for {0}" -f $Label)
        return
    }

    $responseFrame = Read-DoipFrame -Stream $Stream
    if ($responseFrame.PayloadType -ne 0x8001 -or $responseFrame.Payload.Length -lt 5) {
        throw "Received an invalid DoIP Diagnostic Message response for $Label."
    }
    $responseSourceAddress =
        ([int]$responseFrame.Payload[0] -shl 8) -bor [int]$responseFrame.Payload[1]
    $responseTargetAddress =
        ([int]$responseFrame.Payload[2] -shl 8) -bor [int]$responseFrame.Payload[3]
    $actualUdsResponse = [byte[]]$responseFrame.Payload[4..($responseFrame.Payload.Length - 1)]
    if ($responseSourceAddress -ne $EntityAddress -or
        $responseTargetAddress -ne $TesterAddress -or
        [BitConverter]::ToString($actualUdsResponse) -ne
        [BitConverter]::ToString($ExpectedUdsResponse)) {
        throw "UDS response for $Label had unexpected addresses or bytes."
    }
    Write-Output ("UDS response for {0}: {1}" -f
        $Label, [BitConverter]::ToString($actualUdsResponse).Replace('-', ' '))
}

$client = [System.Net.Sockets.TcpClient]::new()
try {
    try {
        $client.Connect($Destination, $Port)
    } catch [System.Net.Sockets.SocketException] {
        throw "Could not connect to DoIP server at ${Destination}:$Port. $($_.Exception.Message)"
    }

    $client.NoDelay = $true
    $stream = $client.GetStream()
    $stream.ReadTimeout = $TimeoutSeconds * 1000

    $testerAddress = 0x0e00
    $entityAddress = 0x1001
    $routingActivation = [byte[]](
        0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    )
    Send-DoipFrame -Stream $stream -PayloadType 0x0005 -Payload $routingActivation

    $activationResponse = Read-DoipFrame -Stream $stream
    if ($activationResponse.PayloadType -ne 0x0006 -or
        $activationResponse.Payload.Length -ne 9) {
        throw "Received an invalid Routing Activation Response."
    }
    $activationTesterAddress =
        ([int]$activationResponse.Payload[0] -shl 8) -bor
        [int]$activationResponse.Payload[1]
    $activationEntityAddress =
        ([int]$activationResponse.Payload[2] -shl 8) -bor
        [int]$activationResponse.Payload[3]
    if ($activationTesterAddress -ne $testerAddress -or
        $activationEntityAddress -ne $entityAddress -or
        $activationResponse.Payload[4] -ne 0x10) {
        throw "Routing activation did not succeed with the expected logical addresses."
    }
    Write-Output ("Routing activation: tester=0x{0:X4} entity=0x{1:X4} code=0x10" -f
        $activationTesterAddress, $activationEntityAddress)

    Send-DiagnosticRequest -Stream $stream -TesterAddress $testerAddress `
        -EntityAddress $entityAddress -UdsRequest ([byte[]](0x10, 0x03)) `
        -ExpectedUdsResponse ([byte[]](0x50, 0x03, 0x00, 0x32, 0x01, 0xf4)) `
        -Label "SessionControl extended"
    Send-DiagnosticRequest -Stream $stream -TesterAddress $testerAddress `
        -EntityAddress $entityAddress -UdsRequest ([byte[]](0x3e, 0x00)) `
        -ExpectedUdsResponse ([byte[]](0x7e, 0x00)) -Label "TesterPresent"
    Send-DiagnosticRequest -Stream $stream -TesterAddress $testerAddress `
        -EntityAddress $entityAddress -UdsRequest ([byte[]](0x3e, 0x80)) `
        -Label "TesterPresent suppressed"
    Send-DiagnosticRequest -Stream $stream -TesterAddress $testerAddress `
        -EntityAddress $entityAddress -UdsRequest ([byte[]](0x11, 0x01)) `
        -ExpectedUdsResponse ([byte[]](0x51, 0x01)) -Label "ECUReset hard reset"
    Write-Output "ECUReset is simulated; the server process remains running."
} finally {
    $client.Dispose()
}
