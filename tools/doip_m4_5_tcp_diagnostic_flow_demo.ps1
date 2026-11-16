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

    $udsData = [byte[]](0x22, 0xf1, 0x90)
    $diagnosticPayload = [byte[]]@(
        [byte](($testerAddress -shr 8) -band 0xff),
        [byte]($testerAddress -band 0xff),
        [byte](($entityAddress -shr 8) -band 0xff),
        [byte]($entityAddress -band 0xff)
    ) + $udsData
    Send-DoipFrame -Stream $stream -PayloadType 0x8001 -Payload $diagnosticPayload

    $acknowledgement = Read-DoipFrame -Stream $stream
    if ($acknowledgement.PayloadType -ne 0x8002 -or
        $acknowledgement.Payload.Length -ne 5) {
        throw "Received an invalid Diagnostic Message acknowledgement."
    }
    $ackSourceAddress =
        ([int]$acknowledgement.Payload[0] -shl 8) -bor [int]$acknowledgement.Payload[1]
    $ackTargetAddress =
        ([int]$acknowledgement.Payload[2] -shl 8) -bor [int]$acknowledgement.Payload[3]
    if ($ackSourceAddress -ne $entityAddress -or
        $ackTargetAddress -ne $testerAddress -or
        $acknowledgement.Payload[4] -ne 0x00) {
        throw "Diagnostic Message acknowledgement had unexpected addresses or code."
    }

    Write-Output ("Diagnostic ACK: source=0x{0:X4} target=0x{1:X4} code=0x00" -f
        $ackSourceAddress, $ackTargetAddress)
    Write-Output ("Acknowledged UDS bytes: {0}" -f
        [BitConverter]::ToString($udsData).Replace('-', ' '))

    $udsResponse = Read-DoipFrame -Stream $stream
    if ($udsResponse.PayloadType -ne 0x8001 -or $udsResponse.Payload.Length -ne 7) {
        throw "Received an invalid UDS Diagnostic Message response."
    }
    $udsResponseSource =
        ([int]$udsResponse.Payload[0] -shl 8) -bor [int]$udsResponse.Payload[1]
    $udsResponseTarget =
        ([int]$udsResponse.Payload[2] -shl 8) -bor [int]$udsResponse.Payload[3]
    if ($udsResponseSource -ne $entityAddress -or
        $udsResponseTarget -ne $testerAddress -or
        [BitConverter]::ToString([byte[]]$udsResponse.Payload[4..6]) -ne '7F-22-11') {
        throw "Received an unexpected UDS negative response."
    }
    Write-Output "UDS response: 7F 22 11 (service 0x22 is not yet implemented)."
} finally {
    $client.Dispose()
}
