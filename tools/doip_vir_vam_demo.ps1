param(
    [string]$Destination = "255.255.255.255",
    [ValidateRange(1, 65535)]
    [int]$Port = 13400,
    [ValidateRange(1, 60)]
    [int]$TimeoutSeconds = 3
)

$client = [System.Net.Sockets.UdpClient]::new()
try {
    $client.EnableBroadcast = $true
    $client.Client.ReceiveTimeout = $TimeoutSeconds * 1000
    $request = [byte[]](0xff, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00)
    [void]$client.Send($request, $request.Length, $Destination, $Port)

    $peer = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0)
    $response = $client.Receive([ref]$peer)
} catch [System.Net.Sockets.SocketException] {
    Write-Error "DoIP discovery failed: $($_.Exception.Message)"
    exit 1
} finally {
    $client.Dispose()
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
Write-Output "VAM response from $($peer.Address):$($peer.Port)"
Write-Output "VIN: $vin"
Write-Output ("Logical address: 0x{0:X4}" -f $logicalAddress)
Write-Output "EID: $eid"
Write-Output "GID: $gid"
