try {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime | Out-Null
    $profile = [Windows.Networking.Connectivity.NetworkInformation, Windows.Networking.Connectivity, ContentType=WindowsRuntime]::GetInternetConnectionProfile()
    if ($null -eq $profile) {
        Write-Output 'STATE=Off'
        exit 0
    }
    $manager = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager, Windows.Networking.NetworkOperators, ContentType=WindowsRuntime]::CreateFromConnectionProfile($profile)
    Write-Output ("STATE={0}" -f $manager.TetheringOperationalState)
    exit 0
} catch {
    Write-Output 'STATE=Off'
    exit 0
}
