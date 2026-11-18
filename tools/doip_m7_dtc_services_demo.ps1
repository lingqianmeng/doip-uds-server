param(
    [string]$Destination = "127.0.0.1",
    [ValidateRange(1, 65535)]
    [int]$Port = 13400,
    [ValidateRange(1, 30)]
    [int]$TimeoutSeconds = 5
)

$ErrorActionPreference = "Stop"

function Read-ExactBytes {
    param([System.Net.Sockets.NetworkStream]$Stream, [int]$Length)

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

    $length = $Payload.Length
    $frame = [byte[]]@(
        0x02, 0xfd,
        [byte](($PayloadType -shr 8) -band 0xff),
        [byte]($PayloadType -band 0xff),
        [byte](($length -shr 24) -band 0xff),
        [byte](($length -shr 16) -band 0xff),
        [byte](($length -shr 8) -band 0xff),
        [byte]($length -band 0xff)
    ) + $Payload
    $Stream.Write($frame, 0, $frame.Length)
    $Stream.Flush()
}

function Send-DiagnosticRequest {
    param(
        [System.Net.Sockets.NetworkStream]$Stream,
        [int]$TesterAddress,
        [int]$EntityAddress,
        [byte[]]$Request,
        [byte[]]$ExpectedResponse,
        [string]$Label
    )

    $payload = [byte[]]@(
        [byte](($TesterAddress -shr 8) -band 0xff),
        [byte]($TesterAddress -band 0xff),
        [byte](($EntityAddress -shr 8) -band 0xff),
        [byte]($EntityAddress -band 0xff)
    ) + $Request
    Send-DoipFrame -Stream $Stream -PayloadType 0x8001 -Payload $payload

    $ack = Read-DoipFrame -Stream $Stream
    if ($ack.PayloadType -ne 0x8002 -or $ack.Payload.Length -ne 5 -or
        $ack.Payload[4] -ne 0x00) {
        throw "Received an invalid Diagnostic Message acknowledgement for $Label."
    }

    $response = Read-DoipFrame -Stream $Stream
    if ($response.PayloadType -ne 0x8001 -or $response.Payload.Length -lt 5) {
        throw "Received an invalid UDS Diagnostic Message response for $Label."
    }
    $source = ([int]$response.Payload[0] -shl 8) -bor [int]$response.Payload[1]
    $target = ([int]$response.Payload[2] -shl 8) -bor [int]$response.Payload[3]
    $actual = [byte[]]$response.Payload[4..($response.Payload.Length - 1)]
    if ($source -ne $EntityAddress -or $target -ne $TesterAddress -or
        [BitConverter]::ToString($actual) -ne [BitConverter]::ToString($ExpectedResponse)) {
        throw "UDS response for $Label had unexpected addresses or bytes: $($actual -join ' ')."
    }
    Write-Output ("{0}: {1}" -f
        $Label, [BitConverter]::ToString($actual).Replace('-', ' '))
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
    Send-DoipFrame -Stream $stream -PayloadType 0x0005 `
        -Payload ([byte[]](0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00))
    $activation = Read-DoipFrame -Stream $stream
    if ($activation.PayloadType -ne 0x0006 -or $activation.Payload.Length -ne 9 -or
        $activation.Payload[4] -ne 0x10) {
        throw "Routing activation did not succeed."
    }
    Write-Output ("Routing activation succeeded for tester 0x{0:X4}." -f $testerAddress)

    $records = [System.Collections.Generic.List[byte]]::new()
    $records.AddRange([byte[]]@(0x59, 0x02, 0xff))
    for ($index = 1; $index -le 10; $index++) {
        $records.AddRange([byte[]]@(0xf0, 0x00, [byte]$index, 0x09))
    }

    Send-DiagnosticRequest -Stream $stream -TesterAddress $testerAddress `
        -EntityAddress $entityAddress -Request ([byte[]](0x19, 0x01, 0x09)) `
        -ExpectedResponse ([byte[]](0x59, 0x01, 0xff, 0x01, 0x00, 0x0a)) `
        -Label "Count DTCs matching status mask 0x09"
    Send-DiagnosticRequest -Stream $stream -TesterAddress $testerAddress `
        -EntityAddress $entityAddress -Request ([byte[]](0x19, 0x02, 0x08)) `
        -ExpectedResponse $records.ToArray() -Label "List DTCs matching status mask 0x08"
    Send-DiagnosticRequest -Stream $stream -TesterAddress $testerAddress `
        -EntityAddress $entityAddress -Request ([byte[]](0x10, 0x03)) `
        -ExpectedResponse ([byte[]](0x50, 0x03, 0x00, 0x32, 0x01, 0xf4)) `
        -Label "Enter extended session"
    Send-DiagnosticRequest -Stream $stream -TesterAddress $testerAddress `
        -EntityAddress $entityAddress -Request ([byte[]](0x14, 0xff, 0xff, 0xff)) `
        -ExpectedResponse ([byte[]](0x54)) -Label "Clear all runtime DTCs"
    Send-DiagnosticRequest -Stream $stream -TesterAddress $testerAddress `
        -EntityAddress $entityAddress -Request ([byte[]](0x19, 0x01, 0xff)) `
        -ExpectedResponse ([byte[]](0x59, 0x01, 0xff, 0x01, 0x00, 0x00)) `
        -Label "Confirm the runtime DTC set is empty"
} finally {
    $client.Dispose()
}
