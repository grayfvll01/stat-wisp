param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$TestDirectory,
    [switch]$Stalled
)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ExitTestNative {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c, string t);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w, uint m, IntPtr p, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w, out uint id);
}
'@
New-Item -ItemType Directory -Force -Path $TestDirectory | Out-Null
$env:STAT_WISP_SETTINGS_PATH = Join-Path $TestDirectory 'settings.ini'
@'
version=2
enabled=cpu.usage,ram.usage
interval_ms=500
first_run_completed=1
start_with_windows=0
'@ | Set-Content -LiteralPath $env:STAT_WISP_SETTINGS_PATH -Encoding ascii
$process = Start-Process -FilePath $Executable -WindowStyle Hidden -PassThru
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 50
        $window = [ExitTestNative]::FindWindow('stat-wisp-message-window', $null)
        [uint32]$owner = 0
        if ($window -ne [IntPtr]::Zero) { [void][ExitTestNative]::GetWindowThreadProcessId($window, [ref]$owner) }
    } while ($owner -ne $process.Id -and [DateTime]::UtcNow -lt $deadline)
    if ($owner -ne $process.Id) { throw 'The test application did not create its own window.' }
    Start-Sleep -Milliseconds 500
    $watch = [Diagnostics.Stopwatch]::StartNew()
    [void][ExitTestNative]::PostMessage($window, 0x111, [IntPtr]4504, [IntPtr]::Zero)
    if (-not $process.WaitForExit(5000)) { throw 'Exit failed to close the process within five seconds.' }
    $expected = if ($Stalled) { 1460 } else { 0 }
    if ($process.ExitCode -ne $expected) { throw "Unexpected exit code: $($process.ExitCode), expected $expected" }
    Write-Output "Exit passed: $($watch.ElapsedMilliseconds) ms; code $($process.ExitCode)."
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    Remove-Item Env:STAT_WISP_SETTINGS_PATH
}
