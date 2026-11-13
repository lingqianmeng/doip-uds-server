param(
    [string]$Destination = "127.0.0.1",
    [ValidateRange(1, 65535)]
    [int]$Port = 13400,
    [ValidateRange(1, 60)]
    [int]$TimeoutSeconds = 3,
    [ValidateRange(1, 60)]
    [int]$HoldSeconds = 5
)

$udpClient = [System.Net.Sockets.UdpClient]::new()
try {
    $udpClient.EnableBroadcast = $true
    $udpClient.Client.ReceiveTimeout = $TimeoutSeconds * 1000
    $request = [byte[]](0xff, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00)
    [void]$udpClient.Send($request, $request.Length, $Destination, $Port)

    $udpPeer = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0)
    $response = $udpClient.Receive([ref]$udpPeer)
} catch [System.Net.Sockets.SocketException] {
    throw "DoIP VIR/VAM exchange failed: $($_.Exception.Message)"
} finally {
    $udpClient.Dispose()
}

if ($response.Length -ne 41 -or
    $response[0] -ne 0x02 -or $response[1] -ne 0xfd -or
    $response[2] -ne 0x00 -or $response[3] -ne 0x04 -or
    $response[4] -ne 0x00 -or $response[5] -ne 0x00 -or
    $response[6] -ne 0x00 -or $response[7] -ne 0x21) {
    throw "Received a datagram that is not a valid vehicle-identification response."
}

$vin = [System.Text.Encoding]::ASCII.GetString($response, 8, 17)
$logicalAddress = ($response[25] -shl 8) -bor $response[26]
$eid = [BitConverter]::ToString($response[27..32])
$gid = [BitConverter]::ToString($response[33..38])
Write-Output "VIR sent to ${Destination}:$Port"
Write-Output "VAM response from $($udpPeer.Address):$($udpPeer.Port)"
Write-Output "VIN: $vin"
Write-Output ("Logical address: 0x{0:X4}" -f $logicalAddress)
Write-Output "EID: $eid"
Write-Output "GID: $gid"

$tcpClient = [System.Net.Sockets.TcpClient]::new()
try {
    $tcpClient.Connect($Destination, $Port)
    Write-Output "TCP connected to ${Destination}:$Port"
    Write-Output "Holding the connection for $HoldSeconds seconds; no DoIP message is sent."
    Start-Sleep -Seconds $HoldSeconds
} catch [System.Net.Sockets.SocketException] {
    throw "DoIP TCP connect failed: $($_.Exception.Message)"
} finally {
    $tcpClient.Dispose()
}

Write-Output "TCP connection closed."
