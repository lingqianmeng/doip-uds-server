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
            throw "Server closed the connection before sending the routing activation response."
        }
        $offset += $read
    }
    return ,$bytes
}

function Connect-DoipServer {
    param(
        [string]$Address,
        [int]$ServerPort
    )

    $client = [System.Net.Sockets.TcpClient]::new()
    try {
        $client.Connect($Address, $ServerPort)
        $client.NoDelay = $true
        return $client
    } catch [System.Net.Sockets.SocketException] {
        $client.Dispose()
        throw "Could not connect to DoIP server at ${Address}:$ServerPort. $($_.Exception.Message)"
    }
}

function Send-RoutingActivationRequest {
    param(
        [System.Net.Sockets.TcpClient]$Client,
        [int]$TesterAddress,
        [int]$ActivationType,
        [int]$Timeout
    )

    $request = [byte[]]@(
        0x02, 0xfd, 0x00, 0x05, 0x00, 0x00, 0x00, 0x07,
        [byte](($TesterAddress -shr 8) -band 0xff),
        [byte]($TesterAddress -band 0xff),
        [byte]$ActivationType,
        0x00, 0x00, 0x00, 0x00
    )
    $stream = $Client.GetStream()
    $stream.ReadTimeout = $Timeout * 1000
    $stream.Write($request, 0, $request.Length)
    $stream.Flush()

    $responseHeader = Read-ExactBytes -Stream $stream -Length 8
    if ($responseHeader[0] -ne 0x02 -or $responseHeader[1] -ne 0xfd -or
        $responseHeader[2] -ne 0x00 -or $responseHeader[3] -ne 0x06) {
        throw "Received an invalid Routing Activation Response header."
    }
    $responsePayloadLength =
        ([int]$responseHeader[4] -shl 24) -bor
        ([int]$responseHeader[5] -shl 16) -bor
        ([int]$responseHeader[6] -shl 8) -bor
        [int]$responseHeader[7]
    if ($responsePayloadLength -ne 9) {
        throw "Routing Activation Response has unexpected payload length $responsePayloadLength."
    }

    $responsePayload = Read-ExactBytes -Stream $stream -Length $responsePayloadLength
    $responseTesterAddress =
        ([int]$responsePayload[0] -shl 8) -bor [int]$responsePayload[1]
    if ($responseTesterAddress -ne $TesterAddress) {
        throw ("Routing Activation Response echoed tester address 0x{0:X4}; expected 0x{1:X4}." -f
            $responseTesterAddress, $TesterAddress)
    }

    return [pscustomobject]@{
        TesterAddress = $responseTesterAddress
        EntityAddress = (([int]$responsePayload[2] -shl 8) -bor [int]$responsePayload[3])
        ResponseCode = [int]$responsePayload[4]
    }
}

function Assert-ActivationResponse {
    param(
        [string]$Description,
        [System.Net.Sockets.TcpClient]$Client,
        [int]$TesterAddress,
        [int]$ActivationType,
        [int]$ExpectedCode,
        [int]$Timeout
    )

    $response = Send-RoutingActivationRequest `
        -Client $Client `
        -TesterAddress $TesterAddress `
        -ActivationType $ActivationType `
        -Timeout $Timeout
    if ($response.ResponseCode -ne $ExpectedCode) {
        throw ("{0}: received response code 0x{1:X2}; expected 0x{2:X2}." -f
            $Description, $response.ResponseCode, $ExpectedCode)
    }
    Write-Output ("{0}: tester=0x{1:X4} entity=0x{2:X4} response=0x{3:X2}" -f
        $Description, $response.TesterAddress, $response.EntityAddress, $response.ResponseCode)
}

$firstClient = $null
$secondClient = $null
try {
    $firstClient = Connect-DoipServer -Address $Destination -ServerPort $Port
    Assert-ActivationResponse `
        -Description "Default activation" `
        -Client $firstClient `
        -TesterAddress 0x0e00 `
        -ActivationType 0x00 `
        -ExpectedCode 0x10 `
        -Timeout $TimeoutSeconds
    Assert-ActivationResponse `
        -Description "Different address on active connection" `
        -Client $firstClient `
        -TesterAddress 0x0e01 `
        -ActivationType 0x00 `
        -ExpectedCode 0x02 `
        -Timeout $TimeoutSeconds
    Assert-ActivationResponse `
        -Description "Unsupported activation type" `
        -Client $firstClient `
        -TesterAddress 0x0e00 `
        -ActivationType 0x01 `
        -ExpectedCode 0x06 `
        -Timeout $TimeoutSeconds

    $secondClient = Connect-DoipServer -Address $Destination -ServerPort $Port
    Assert-ActivationResponse `
        -Description "Tester address active on another connection" `
        -Client $secondClient `
        -TesterAddress 0x0e00 `
        -ActivationType 0x00 `
        -ExpectedCode 0x03 `
        -Timeout $TimeoutSeconds
    Assert-ActivationResponse `
        -Description "Distinct tester address on second connection" `
        -Client $secondClient `
        -TesterAddress 0x0e01 `
        -ActivationType 0x00 `
        -ExpectedCode 0x10 `
        -Timeout $TimeoutSeconds
    Assert-ActivationResponse `
        -Description "Address outside test-equipment range" `
        -Client $secondClient `
        -TesterAddress 0x0dff `
        -ActivationType 0x00 `
        -ExpectedCode 0x00 `
        -Timeout $TimeoutSeconds
} finally {
    if ($secondClient) {
        $secondClient.Dispose()
    }
    if ($firstClient) {
        $firstClient.Dispose()
    }
}
