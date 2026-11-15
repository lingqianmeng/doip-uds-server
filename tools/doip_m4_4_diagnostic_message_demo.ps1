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
    $payload = Read-ExactBytes -Stream $Stream -Length $payloadLength
    return [pscustomobject]@{
        PayloadType = $payloadType
        Payload = $payload
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
        [int]$SourceAddress,
        [int]$TargetAddress,
        [byte[]]$UdsData
    )

    $payload = [byte[]]@(
        [byte](($SourceAddress -shr 8) -band 0xff),
        [byte]($SourceAddress -band 0xff),
        [byte](($TargetAddress -shr 8) -band 0xff),
        [byte]($TargetAddress -band 0xff)
    ) + $UdsData
    Send-DoipFrame -Stream $Stream -PayloadType 0x8001 -Payload $payload
}

function Assert-DiagnosticResponse {
    param(
        [string]$Description,
        [System.Net.Sockets.NetworkStream]$Stream,
        [int]$ExpectedPayloadType,
        [int]$ExpectedCode,
        [int]$ExpectedEntityAddress,
        [int]$ExpectedTesterAddress
    )

    $response = Read-DoipFrame -Stream $Stream
    if ($response.PayloadType -ne $ExpectedPayloadType) {
        throw ("{0}: received payload type 0x{1:X4}; expected 0x{2:X4}." -f
            $Description, $response.PayloadType, $ExpectedPayloadType)
    }
    if ($response.Payload.Length -ne 5) {
        throw ("{0}: expected a 5-byte ACK/NACK payload, received {1} bytes." -f
            $Description, $response.Payload.Length)
    }

    $sourceAddress = ([int]$response.Payload[0] -shl 8) -bor [int]$response.Payload[1]
    $targetAddress = ([int]$response.Payload[2] -shl 8) -bor [int]$response.Payload[3]
    $code = [int]$response.Payload[4]
    if ($sourceAddress -ne $ExpectedEntityAddress -or
        $targetAddress -ne $ExpectedTesterAddress -or
        $code -ne $ExpectedCode) {
        throw ("{0}: unexpected response addresses/code source=0x{1:X4} target=0x{2:X4} code=0x{3:X2}." -f
            $Description, $sourceAddress, $targetAddress, $code)
    }
    Write-Output ("{0}: type=0x{1:X4} source=0x{2:X4} target=0x{3:X4} code=0x{4:X2}" -f
        $Description, $response.PayloadType, $sourceAddress, $targetAddress, $code)
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

    Send-DiagnosticRequest -Stream $stream `
        -SourceAddress 0x0e00 `
        -TargetAddress 0x1001 `
        -UdsData ([byte[]](0x3e, 0x00))
    Assert-DiagnosticResponse `
        -Description "Diagnostic message before activation" `
        -Stream $stream `
        -ExpectedPayloadType 0x8003 `
        -ExpectedCode 0x02 `
        -ExpectedEntityAddress 0x1001 `
        -ExpectedTesterAddress 0x0e00

    $routingActivation = [byte[]](
        0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    )
    Send-DoipFrame -Stream $stream -PayloadType 0x0005 -Payload $routingActivation
    $activationResponse = Read-DoipFrame -Stream $stream
    if ($activationResponse.PayloadType -ne 0x0006 -or
        $activationResponse.Payload.Length -ne 9 -or
        $activationResponse.Payload[4] -ne 0x10) {
        throw "Default routing activation did not succeed; cannot run diagnostic-message checks."
    }
    Write-Output "Default routing activation succeeded for tester 0x0E00."

    Send-DiagnosticRequest -Stream $stream `
        -SourceAddress 0x0e01 `
        -TargetAddress 0x1001 `
        -UdsData ([byte[]](0x3e, 0x00))
    Assert-DiagnosticResponse `
        -Description "Source address does not match activation" `
        -Stream $stream `
        -ExpectedPayloadType 0x8003 `
        -ExpectedCode 0x02 `
        -ExpectedEntityAddress 0x1001 `
        -ExpectedTesterAddress 0x0e01

    Send-DiagnosticRequest -Stream $stream `
        -SourceAddress 0x0e00 `
        -TargetAddress 0x1002 `
        -UdsData ([byte[]](0x22, 0xf1, 0x90))
    Assert-DiagnosticResponse `
        -Description "Target address is not this ECU" `
        -Stream $stream `
        -ExpectedPayloadType 0x8003 `
        -ExpectedCode 0x03 `
        -ExpectedEntityAddress 0x1001 `
        -ExpectedTesterAddress 0x0e00

    Send-DiagnosticRequest -Stream $stream `
        -SourceAddress 0x0e00 `
        -TargetAddress 0x1001 `
        -UdsData ([byte[]](0x22, 0xf1, 0x90))
    Assert-DiagnosticResponse `
        -Description "Valid diagnostic message" `
        -Stream $stream `
        -ExpectedPayloadType 0x8002 `
        -ExpectedCode 0x00 `
        -ExpectedEntityAddress 0x1001 `
        -ExpectedTesterAddress 0x0e00
    Write-Output "The accepted UDS bytes were acknowledged but not interpreted."
} finally {
    $client.Dispose()
}
