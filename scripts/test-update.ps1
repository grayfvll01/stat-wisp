param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$TestDirectory
)
# End-to-end update check: a local HTTP server publishes a newer release; a
# portable copy must reject a tampered download, then install a verified one
# in place and restart itself.
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class UpdateTestNative {
    private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr window, StringBuilder name, int count);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    public static IntPtr FindProcessWindow(uint processId, string className) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, parameter) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            var name = new StringBuilder(256);
            if (owner == processId && GetClassName(window, name, name.Capacity) != 0 && name.ToString() == className) {
                found = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@

function Wait-Until([scriptblock]$Condition, [string]$Failure, [int]$Seconds = 30) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw $Failure
}

function Send-Command([int]$ProcessId, [int]$Command) {
    $window = [UpdateTestNative]::FindProcessWindow($ProcessId, 'stat-wisp-message-window')
    if ($window -eq [IntPtr]::Zero) { throw "Process $ProcessId has no message window." }
    [void][UpdateTestNative]::PostMessage($window, 0x111, [IntPtr]$Command, [IntPtr]::Zero)
}

function Write-Manifest([string]$Path, [string]$PortableHash, [long]$PortableSize) {
    @"
format=1
version=99.0.0
installer=stat-wisp-99.0.0-setup.exe
installer_sha256=$PortableHash
installer_size=$PortableSize
portable=stat-wisp.exe
portable_sha256=$PortableHash
portable_size=$PortableSize
"@ | Set-Content -LiteralPath $Path -Encoding ascii
}

if (Get-Process -Name stat-wisp -ErrorAction SilentlyContinue) { throw 'Exit Stat Wisp before running the update test.' }
$root = (New-Item -ItemType Directory -Force -Path $TestDirectory).FullName
$portable = Join-Path $root 'portable'
$server = Join-Path $root 'server'
Remove-Item -LiteralPath $portable, $server -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $portable, $server | Out-Null
$installed = Join-Path $portable 'stat-wisp.exe'
Copy-Item -LiteralPath $Executable -Destination $installed

# The "new release" is the same program with extra overlay bytes, so it runs
# normally but has a different hash that proves the file was replaced.
$release = Join-Path $server 'stat-wisp.exe'
Copy-Item -LiteralPath $Executable -Destination $release
[IO.File]::AppendAllText($release, 'stat-wisp update test overlay')
$releaseHash = (Get-FileHash -LiteralPath $release -Algorithm SHA256).Hash.ToLowerInvariant()
$releaseSize = (Get-Item -LiteralPath $release).Length
$manifest = Join-Path $server 'stat-wisp-update.ini'
Write-Manifest $manifest ('0' * 64) $releaseSize

$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
$listener.Start()
$port = $listener.LocalEndpoint.Port
$serverJob = Start-ThreadJob -ArgumentList $listener, $server -ScriptBlock {
    param($listener, $root)
    while ($true) {
        try { $client = $listener.AcceptTcpClient() } catch { return }
        try {
            $stream = $client.GetStream()
            $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::ASCII, $false, 1024, $true)
            $requestLine = $reader.ReadLine()
            while ($reader.ReadLine()) { }
            $name = [IO.Path]::GetFileName(($requestLine -split ' ')[1])
            $file = Join-Path $root $name
            if ($name -and (Test-Path -LiteralPath $file -PathType Leaf)) {
                $body = [IO.File]::ReadAllBytes($file)
                $header = "HTTP/1.1 200 OK`r`nContent-Type: application/octet-stream`r`nContent-Length: $($body.Length)`r`nConnection: close`r`n`r`n"
            } else {
                $body = [byte[]]@()
                $header = "HTTP/1.1 404 Not Found`r`nContent-Length: 0`r`nConnection: close`r`n`r`n"
            }
            $bytes = [Text.Encoding]::ASCII.GetBytes($header)
            $stream.Write($bytes, 0, $bytes.Length)
            $stream.Write($body, 0, $body.Length)
            $stream.Flush()
        } catch { } finally { $client.Dispose() }
    }
}

$previousSettings = $env:STAT_WISP_SETTINGS_PATH
$previousBase = $env:STAT_WISP_UPDATE_BASE_URL
$processes = @()
try {
    $data = Join-Path $root 'data'
    New-Item -ItemType Directory -Force -Path $data | Out-Null
    $env:STAT_WISP_SETTINGS_PATH = Join-Path $data 'settings.ini'
    $env:STAT_WISP_UPDATE_BASE_URL = "http://127.0.0.1:$port"
    $log = Join-Path $data 'stat-wisp.log'
    @'
version=3
enabled=cpu.usage
interval_ms=1000
start_with_windows=0
check_for_updates=0
first_run_completed=1
'@ | Set-Content -LiteralPath $env:STAT_WISP_SETTINGS_PATH -Encoding ascii
    $logText = { if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' } }

    $first = Start-Process -FilePath $installed -PassThru
    $processes += $first
    Wait-Until { [UpdateTestNative]::FindProcessWindow($first.Id, 'stat-wisp-message-window') -ne [IntPtr]::Zero } 'The app did not start.'

    Send-Command $first.Id 4505
    Wait-Until { (& $logText) -match 'Update available: 99\.0\.0' } 'The newer release was not detected.'

    # A download that does not match the published hash must never be installed.
    $original = (Get-FileHash -LiteralPath $installed).Hash
    Send-Command $first.Id 4506
    Wait-Until { (& $logText) -match 'Update installation failed: downloaded file failed size or SHA-256 verification' } 'A tampered download was not rejected.'
    if ($first.HasExited -or (Get-FileHash -LiteralPath $installed).Hash -ne $original) { throw 'A tampered download changed the installation.' }
    Write-Output 'Tampered download rejected; installation unchanged.'

    Write-Manifest $manifest $releaseHash $releaseSize
    Send-Command $first.Id 4505
    Wait-Until { ([regex]::Matches((& $logText), 'Update available: 99\.0\.0')).Count -ge 2 } 'The corrected release was not read.'
    Send-Command $first.Id 4506
    Wait-Until { $first.HasExited } 'The app did not exit to finish the update.'
    if ($first.ExitCode -ne 0) { throw "Unexpected exit code after update: $($first.ExitCode)" }
    Wait-Until { (Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash.ToLowerInvariant() -eq $releaseHash } 'The executable was not replaced.'
    $second = $null
    Wait-Until {
        $script:second = Get-Process -Name stat-wisp -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $installed } | Select-Object -First 1
        $script:second -and [UpdateTestNative]::FindProcessWindow($script:second.Id, 'stat-wisp-message-window') -ne [IntPtr]::Zero
    } 'The updated app did not restart.'
    $processes += $second
    Wait-Until { -not (Test-Path -LiteralPath "$installed.old") } 'The previous executable was not cleaned up.'
    Write-Output 'Verified update installed in place, restarted, and cleaned up.'

    Send-Command $second.Id 4504
    Wait-Until { $second.HasExited } 'The updated app did not exit.' 10
    Write-Output 'Update end-to-end test passed.'
} finally {
    foreach ($process in $processes) {
        if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
    }
    $listener.Stop()
    Stop-Job $serverJob -ErrorAction SilentlyContinue
    Remove-Job $serverJob -Force -ErrorAction SilentlyContinue
    $env:STAT_WISP_SETTINGS_PATH = $previousSettings
    $env:STAT_WISP_UPDATE_BASE_URL = $previousBase
}
