try {
    $ips = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object {
        $_.IPAddress -like '192.168.137.*'
    }
    $ip = ($ips | Sort-Object PrefixLength -Descending | Select-Object -First 1).IPAddress
    if (-not $ip) {
        $candidates = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object {
            ($_.InterfaceAlias -like 'Local Area Connection*') -and
            ($_.IPAddress -like '192.168.*') -and
            ($_.IPAddress -ne '0.0.0.0')
        }
        $ip = ($candidates | Select-Object -First 1).IPAddress
    }
    if (-not $ip) { $ip = '192.168.137.1' }
    Write-Output ("IP={0}" -f $ip)
} catch {
    Write-Output 'IP=192.168.137.1'
}
