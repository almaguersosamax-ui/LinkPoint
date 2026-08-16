$ErrorActionPreference = 'Stop'
try {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime | Out-Null
    $netInfo = [Windows.Networking.Connectivity.NetworkInformation, Windows.Networking.Connectivity, ContentType=WindowsRuntime]
    $managerType = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager, Windows.Networking.NetworkOperators, ContentType=WindowsRuntime]

    $profiles = @($netInfo::GetConnectionProfiles())
    $internet = $netInfo::GetInternetConnectionProfile()
    if ($null -ne $internet -and -not ($profiles -contains $internet)) {
        $profiles += $internet
    }
    if ($profiles.Count -eq 0) {
        Write-Output 'CAPABILITY=NoConnectionProfile'
        exit 0
    }

    $bestCapability = $null
    foreach ($p in $profiles) {
        try {
            $manager = $managerType::CreateFromConnectionProfile($p)
            $cap = $manager.GetTetheringCapability().ToString()
            if ($null -eq $bestCapability) { $bestCapability = $cap }
            if ($cap -eq 'Enabled') {
                Write-Output 'CAPABILITY=Enabled'
                exit 0
            }
        } catch {
            # perfil no utilizable para tethering: probar el siguiente
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
