param(
    [string]$Destination = "127.0.0.1",
    [ValidateRange(1, 65535)]
    [int]$Port = 13400,
    [ValidateRange(1, 30)]
    [int]$TimeoutSeconds = 5
)

$ErrorActionPreference = "Stop"

function Wait-ForRemoteClose {
    param(
        [System.Net.Sockets.TcpClient]$Client,
        [int]$Timeout
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($Timeout)
    do {
        if ($Client.Client.Poll(100000, [System.Net.Sockets.SelectMode]::SelectRead) -and
            $Client.Available -eq 0) {
            return $true
        }
        Start-Sleep -Milliseconds 20
    } while ([DateTime]::UtcNow -lt $deadline)

    return $false
}

$routingActivation = [byte[]](
    0x02, 0xfd, 0x00, 0x05, 0x00, 0x00, 0x00, 0x07,
    0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
)
$diagnosticTesterPresent = [byte[]](
    0x02, 0xfd, 0x80, 0x01, 0x00, 0x00, 0x00, 0x06,
    0x0e, 0x00, 0x00, 0x01, 0x3e, 0x00
)
$diagnosticSessionControl = [byte[]](
    0x02, 0xfd, 0x80, 0x01, 0x00, 0x00, 0x00, 0x06,
    0x0e, 0x00, 0x00, 0x01, 0x10, 0x01
)

$normalClient = [System.Net.Sockets.TcpClient]::new()
try {
    try {
        $normalClient.Connect($Destination, $Port)
    } catch [System.Net.Sockets.SocketException] {
        throw "Could not connect to DoIP server at ${Destination}:$Port. $($_.Exception.Message)"
    }

    $normalClient.NoDelay = $true
    $stream = $normalClient.GetStream()

    $stream.Write($routingActivation, 0, 3)
    $stream.Flush()
    Start-Sleep -Milliseconds 150
    $stream.Write($routingActivation, 3, $routingActivation.Length - 3)
    $stream.Flush()
    Write-Output "Sent one routing-activation-shaped message in two TCP writes."

    $coalescedMessages = [byte[]]($diagnosticTesterPresent + $diagnosticSessionControl)
    $stream.Write($coalescedMessages, 0, $coalescedMessages.Length)
    $stream.Flush()
    Write-Output "Sent two complete diagnostic-shaped messages in one TCP write."
    Write-Output "No protocol response is expected; this milestone demonstrates framing only."

    Start-Sleep -Milliseconds 300
    if (Wait-ForRemoteClose -Client $normalClient -Timeout 1) {
        throw "The server closed the normal-message connection unexpectedly."
    }
    Write-Output "Normal-message connection remained open."
} finally {
    $normalClient.Dispose()
}

$oversizedPayloadLength = 16 * 1024 * 1024 + 1
$lengthBytes = [byte[]]@(
    [byte](($oversizedPayloadLength -shr 24) -band 0xff),
    [byte](($oversizedPayloadLength -shr 16) -band 0xff),
    [byte](($oversizedPayloadLength -shr 8) -band 0xff),
    [byte]($oversizedPayloadLength -band 0xff)
)
$oversizedHeader = [byte[]](@(0x02, 0xfd, 0x00, 0x05) + $lengthBytes)
$oversizedClient = [System.Net.Sockets.TcpClient]::new()
try {
    try {
        $oversizedClient.Connect($Destination, $Port)
    } catch [System.Net.Sockets.SocketException] {
        throw "Could not connect to DoIP server at ${Destination}:$Port. $($_.Exception.Message)"
    }

    $oversizedClient.GetStream().Write($oversizedHeader, 0, $oversizedHeader.Length)
    Write-Output "Sent only a DoIP header declaring $oversizedPayloadLength payload bytes (limit: 16777216)."

    if (-not (Wait-ForRemoteClose -Client $oversizedClient -Timeout $TimeoutSeconds)) {
        throw "The server did not close the connection with an oversized payload declaration."
    }
    Write-Output "Server rejected the oversized declaration and closed that client connection."
} finally {
    $oversizedClient.Dispose()
}
