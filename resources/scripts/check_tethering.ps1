$ErrorActionPreference = 'Stop'
try {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime | Out-Null
    $profile = [Windows.Networking.Connectivity.NetworkInformation, Windows.Networking.Connectivity, ContentType=WindowsRuntime]::GetInternetConnectionProfile()
    if ($null -eq $profile) {
        Write-Output 'CAPABILITY=NoConnectionProfile'
        exit 0
    }
    $manager = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager, Windows.Networking.NetworkOperators, ContentType=WindowsRuntime]::CreateFromConnectionProfile($profile)
    $cap = $manager.GetTetheringCapability()
    Write-Output ("CAPABILITY={0}" -f $cap)
    exit 0
} catch {
    Write-Output ("ERROR=" + $_.Exception.Message)
    exit 1
}
