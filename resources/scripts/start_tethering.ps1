$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Runtime.WindowsRuntime | Out-Null

$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and
    $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'
})[0]

$asTaskAction = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and
    $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncAction'
})[0]

function Await-Operation($WinRtTask, $ResultType) {
    $asTask = $asTaskGeneric.MakeGenericMethod($ResultType)
    $netTask = $asTask.Invoke($null, @($WinRtTask))
    $netTask.Wait(-1) | Out-Null
    return $netTask.Result
}

function Await-Action($WinRtTask) {
    $netTask = $asTaskAction.Invoke($null, @($WinRtTask))
    $netTask.Wait(-1) | Out-Null
}

try {
    $profile = [Windows.Networking.Connectivity.NetworkInformation, Windows.Networking.Connectivity, ContentType=WindowsRuntime]::GetInternetConnectionProfile()
    if ($null -eq $profile) {
        Write-Output 'ERROR=NoConnectionProfile'
        exit 2
    }
    $manager = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager, Windows.Networking.NetworkOperators, ContentType=WindowsRuntime]::CreateFromConnectionProfile($profile)

    $cfgType = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringAccessPointConfiguration, Windows.Networking.NetworkOperators, ContentType=WindowsRuntime]
    $cfg = [Activator]::CreateInstance($cfgType)
    $cfg.Ssid = $env:HOTSPOT_SSID
    $cfg.Passphrase = $env:HOTSPOT_PASS

    Await-Action ($manager.ConfigureAccessPointAsync($cfg)) | Out-Null

    $result = Await-Operation ($manager.StartTetheringAsync()) ([Windows.Networking.NetworkOperators.NetworkOperatorTetheringOperationResult])
    Write-Output ("STATUS={0}" -f $result.Status)
    if ($result.Status -ne 'Success') {
        exit 1
    }
    exit 0
} catch {
    Write-Output ("ERROR=" + $_.Exception.Message)
    exit 1
}
