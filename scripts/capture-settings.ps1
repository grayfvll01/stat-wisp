param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$Output
)
# Captures the Settings window for documentation using an isolated settings file.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CaptureNative {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c, string t);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr w, uint m, IntPtr p, IntPtr l);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr w, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr w, int attribute, out RECT r, int size);
}
'@
# Measure in physical pixels, like the per-monitor-aware application.
[void][CaptureNative]::SetThreadDpiAwarenessContext([IntPtr]-4)
$directory = Join-Path ([IO.Path]::GetTempPath()) ('stat-wisp-capture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$previous = $env:STAT_WISP_SETTINGS_PATH
$env:STAT_WISP_SETTINGS_PATH = Join-Path $directory 'settings.ini'
@'
version=3
enabled=cpu.temperature,gpu.temperature,cpu.usage,ram.usage
interval_ms=1000
round_values=1
check_for_updates=1
first_run_completed=1
'@ | Set-Content -LiteralPath $env:STAT_WISP_SETTINGS_PATH -Encoding ascii
$process = Start-Process -FilePath $Executable -PassThru
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    do { Start-Sleep -Milliseconds 100; $message = [CaptureNative]::FindWindow('stat-wisp-message-window', [NullString]::Value) }
    while ($message -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
    if ($message -eq [IntPtr]::Zero) { throw 'Stat Wisp did not start.' }
    [void][CaptureNative]::PostMessage($message, 0x111, [IntPtr]4502, [IntPtr]::Zero)
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do { Start-Sleep -Milliseconds 100; $window = [CaptureNative]::FindWindow('stat-wisp-settings', [NullString]::Value) }
    while ($window -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
    if ($window -eq [IntPtr]::Zero) { throw 'Settings did not open.' }
    Start-Sleep -Milliseconds 800
    $rect = New-Object CaptureNative+RECT
    $frame = New-Object CaptureNative+RECT
    [void][CaptureNative]::GetWindowRect($window, [ref]$rect)
    # DWMWA_EXTENDED_FRAME_BOUNDS excludes the invisible resize border.
    if ([CaptureNative]::DwmGetWindowAttribute($window, 9, [ref]$frame, 16) -ne 0) { $frame = $rect }
    $full = New-Object Drawing.Bitmap ($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top)
    $graphics = [Drawing.Graphics]::FromImage($full)
    $dc = $graphics.GetHdc()
    [void][CaptureNative]::PrintWindow($window, $dc, 2)
    $graphics.ReleaseHdc($dc)
    $graphics.Dispose()
    $crop = New-Object Drawing.Rectangle ($frame.Left - $rect.Left), ($frame.Top - $rect.Top), ($frame.Right - $frame.Left), ($frame.Bottom - $frame.Top)
    $bitmap = $full.Clone($crop, $full.PixelFormat)
    $full.Dispose()
    $bitmap.Save($Output, [Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
    Write-Output "Saved $Output"
} finally {
    $message = [CaptureNative]::FindWindow('stat-wisp-message-window', [NullString]::Value)
    if ($message -ne [IntPtr]::Zero) { [void][CaptureNative]::PostMessage($message, 0x111, [IntPtr]4504, [IntPtr]::Zero) }
    if (-not $process.WaitForExit(5000)) { Stop-Process -Id $process.Id -Force }
    $env:STAT_WISP_SETTINGS_PATH = $previous
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
