param(
    [string]$Destination = "127.0.0.1",
    [ValidateRange(1, 65535)]
    [int]$Port = 13400,
    [ValidateRange(1, 60)]
    [int]$HoldSeconds = 3
)

$client = [System.Net.Sockets.TcpClient]::new()
try {
    $client.Connect($Destination, $Port)
    Write-Output "TCP connected to ${Destination}:$Port"
    Write-Output "Holding the connection for $HoldSeconds seconds; no DoIP message is sent."
    Start-Sleep -Seconds $HoldSeconds
} catch [System.Net.Sockets.SocketException] {
    Write-Error "TCP connection failed: $($_.Exception.Message)"
    exit 1
} finally {
    $client.Dispose()
}

Write-Output "TCP connection closed."
