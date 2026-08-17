$ErrorActionPreference = 'Stop'
try {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime | Out-Null
    $netInfo = [Windows.Networking.Connectivity.NetworkInformation, Windows.Networking.Connectivity, ContentType=WindowsRuntime]
    $managerType = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager, Windows.Networking.NetworkOperators, ContentType=WindowsRuntime]

    $internet = $netInfo::GetInternetConnectionProfile()
    if ($null -eq $internet) {
        Write-Output 'INTERNET=None'
    } else {
        Write-Output ("INTERNET={0}|{1}" -f $internet.ProfileName, $internet.GetNetworkConnectivityLevel())
    }

    $profiles = @($netInfo::GetConnectionProfiles()) | Where-Object { $_ -ne $null }
    Write-Output ("PROFILES={0}" -f $profiles.Count)

    if ($profiles.Count -eq 0) {
        Write-Output 'CAPABILITY=NoConnectionProfile'
        exit 0
    }

    $bestCapability = $null
    foreach ($p in $profiles) {
        $name = if ($null -eq $p.ProfileName) { '(sin nombre)' } else { $p.ProfileName }
        $level = $p.GetNetworkConnectivityLevel().ToString()
        try {
            $manager = $managerType::CreateFromConnectionProfile($p)
            $cap = $manager.GetTetheringCapability().ToString()
            Write-Output ("PROFILE={0}|{1}|{2}" -f $name, $level, $cap)
            if ($null -eq $bestCapability) { $bestCapability = $cap }
            if ($cap -eq 'Enabled') {
                Write-Output 'CAPABILITY=Enabled'
                exit 0
            }
        } catch {
            Write-Output ("PROFILE_ERROR={0}|{1}|{2}" -f $name, $level, $_.Exception.Message)
        }
    }
    if ($null -ne $bestCapability) {
        Write-Output ("CAPABILITY={0}" -f $bestCapability)
        exit 0
    }
    Write-Output 'CAPABILITY=NoConnectionProfile'
    exit 0
} catch {
    Write-Output ("ERROR=" + $_.Exception.Message)
    exit 1
}
