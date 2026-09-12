param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$TestDirectory
)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class SettingsDialogTestNative {
    private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr window, StringBuilder name, int count);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageTimeout(IntPtr window, uint message, IntPtr wParam, IntPtr lParam, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int id);
    public static IntPtr[] FindWindows(uint processId, string className) {
        var result = new List<IntPtr>();
        EnumWindows((window, parameter) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            var name = new StringBuilder(256);
            if (owner == processId && GetClassName(window, name, name.Capacity) != 0 && name.ToString() == className)
                result.Add(window);
            return true;
        }, IntPtr.Zero);
        return result.ToArray();
    }
}
'@

function Wait-ForCondition([scriptblock]$Condition, [string]$Failure) {
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    do {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw $Failure
}

$previousSettingsPath = $env:STAT_WISP_SETTINGS_PATH
$process = $null
$existingInstance = $null
try {
    $existingInstance = [Threading.Mutex]::OpenExisting('Local\stat-wisp-6E48CF9D-44ED-4B20-AE74-6C7B52305FE4')
} catch [Threading.WaitHandleCannotBeOpenedException] {
    # No running instance owns the application mutex.
}
if ($existingInstance) {
    $existingInstance.Dispose()
    throw 'Another Stat Wisp instance is running. The modal test will not interact with it.'
}

$runKeyPath = 'Software\Microsoft\Windows\CurrentVersion\Run'
$originalStartupPresent = $false
$originalStartupValue = $null
$originalStartupKind = $null
$runKey = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($runKeyPath)
if ($runKey) {
    try {
        $originalStartupPresent = $runKey.GetValueNames() -contains 'stat-wisp'
        if ($originalStartupPresent) {
            $originalStartupValue = $runKey.GetValue('stat-wisp', $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            $originalStartupKind = $runKey.GetValueKind('stat-wisp')
        }
    } finally { $runKey.Dispose() }
}
try {
    New-Item -ItemType Directory -Force -Path $TestDirectory | Out-Null
    $env:STAT_WISP_SETTINGS_PATH = Join-Path (Resolve-Path -LiteralPath $TestDirectory).Path 'settings.ini'
    @'
version=2
enabled=ram.usage
interval_ms=30000
first_run_completed=1
start_with_windows=0
fahrenheit=0
'@ | Set-Content -LiteralPath $env:STAT_WISP_SETTINGS_PATH -Encoding ascii
    $process = Start-Process -FilePath $Executable -WindowStyle Hidden -PassThru
    Wait-ForCondition {
        if ($process.HasExited) { throw 'The test app exited before opening its own window. Another Stat Wisp instance may be running.' }
        @( [SettingsDialogTestNative]::FindWindows($process.Id, 'stat-wisp-message-window') ).Count -eq 1
    } 'The test application did not create its own message window.'
    $window = [SettingsDialogTestNative]::FindWindows($process.Id, 'stat-wisp-message-window')[0]
    [void][SettingsDialogTestNative]::PostMessage($window, 0x111, [IntPtr]4502, [IntPtr]::Zero)
    Wait-ForCondition {
        @( [SettingsDialogTestNative]::FindWindows($process.Id, 'stat-wisp-settings') ).Count -eq 1
    } 'Settings did not open.'

    # Posted shell commands remain deliverable to the disabled modal owner.
    [void][SettingsDialogTestNative]::PostMessage($window, 0x111, [IntPtr]4502, [IntPtr]::Zero)
    [void][SettingsDialogTestNative]::PostMessage($window, 0x111, [IntPtr]4200, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 300
    $dialogs = @( [SettingsDialogTestNative]::FindWindows($process.Id, 'stat-wisp-settings') )
    if ($dialogs.Count -ne 1) { throw 'A second settings dialog opened during the modal session.' }
    if ((Get-Content -LiteralPath $env:STAT_WISP_SETTINGS_PATH -Raw) -notmatch '(?m)^interval_ms=30000\r?$') {
        throw 'A tray command changed settings while the modal dialog was open.'
    }
    $fahrenheit = [SettingsDialogTestNative]::GetDlgItem($dialogs[0], 2102)
    if ($fahrenheit -eq [IntPtr]::Zero) { throw 'The Fahrenheit control was not found.' }
    [IntPtr]$messageResult = [IntPtr]::Zero
    if ([SettingsDialogTestNative]::SendMessageTimeout($fahrenheit, 0xF1, [IntPtr]1, [IntPtr]::Zero, 2, 1000, [ref]$messageResult) -eq [IntPtr]::Zero) {
        throw 'The settings control did not respond.'
    }
    [void][SettingsDialogTestNative]::PostMessage($dialogs[0], 0x111, [IntPtr]1, [IntPtr]::Zero)
    Wait-ForCondition {
        @( [SettingsDialogTestNative]::FindWindows($process.Id, 'stat-wisp-settings') ).Count -eq 0 -and
        (Get-Content -LiteralPath $env:STAT_WISP_SETTINGS_PATH -Raw) -match '(?m)^fahrenheit=1\r?$'
    } 'Saving settings did not close the dialog and persist the chosen value.'

    [void][SettingsDialogTestNative]::PostMessage($window, 0x111, [IntPtr]4502, [IntPtr]::Zero)
    Wait-ForCondition {
        if ($process.HasExited) { throw "The app exited after saving settings (code $($process.ExitCode))." }
        @( [SettingsDialogTestNative]::FindWindows($process.Id, 'stat-wisp-settings') ).Count -eq 1
    } 'Settings could not reopen after saving.'
    [void][SettingsDialogTestNative]::PostMessage($window, 0x111, [IntPtr]4504, [IntPtr]::Zero)
    if (-not $process.WaitForExit(5000)) { throw 'Exit failed with the settings dialog open.' }
    if ($process.ExitCode -ne 0) { throw "Unexpected exit code: $($process.ExitCode)." }
    Write-Output 'Settings modal regression passed: one dialog, isolated edits, Save, reopen, and Exit.'
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        [void]$process.WaitForExit(5000)
    }
    if ($null -eq $previousSettingsPath) {
        Remove-Item Env:STAT_WISP_SETTINGS_PATH -ErrorAction SilentlyContinue
    } else {
        $env:STAT_WISP_SETTINGS_PATH = $previousSettingsPath
    }
    # A future regression may process the blocked command and touch startup.
    # Restore the exact original value even when an assertion fails.
    $runKey = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($runKeyPath, $true)
    try {
        $currentPresent = $runKey -and ($runKey.GetValueNames() -contains 'stat-wisp')
        if ($originalStartupPresent) {
            $currentValue = if ($currentPresent) {
                $runKey.GetValue('stat-wisp', $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            } else { $null }
            if (-not $currentPresent -or "$currentValue" -cne "$originalStartupValue" -or $runKey.GetValueKind('stat-wisp') -ne $originalStartupKind) {
                if (-not $runKey) { $runKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($runKeyPath) }
                $runKey.SetValue('stat-wisp', $originalStartupValue, $originalStartupKind)
            }
        } elseif ($currentPresent) {
            $runKey.DeleteValue('stat-wisp', $false)
        }
    } finally { if ($runKey) { $runKey.Dispose() } }
}
