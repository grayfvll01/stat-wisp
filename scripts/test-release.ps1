param(
    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version,
    [string]$DistDirectory = (Join-Path $PSScriptRoot '..\dist'),
    [switch]$Install,
    [string]$PreviousInstaller
)

$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$dist = (Resolve-Path -LiteralPath $DistDirectory).Path
$expected = @('LICENSE.txt', "stat-wisp-$Version-setup.exe", "stat-wisp-$Version-win-x64.zip", 'stat-wisp.exe')
$actual = @(Get-ChildItem -LiteralPath $dist -File | ForEach-Object Name)
if (Compare-Object ($expected + 'SHA256SUMS.txt' | Sort-Object) ($actual | Sort-Object)) {
    throw 'The release must contain exactly the executable, installer, portable ZIP, license, and checksum file.'
}
$seen = @{}
foreach ($line in Get-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.txt')) {
    if ($line -notmatch '^([a-f0-9]{64})  ([^/\\]+)$') { throw "Invalid checksum line: $line" }
    $digest, $name = $Matches[1], $Matches[2]
    if ($name -notin $expected -or $seen.ContainsKey($name)) { throw "Unexpected or duplicate checksum: $name" }
    if ((Get-FileHash -LiteralPath (Join-Path $dist $name) -Algorithm SHA256).Hash -ne $digest) {
        throw "Checksum mismatch: $name"
    }
    $seen[$name] = $true
}
if ($seen.Count -ne $expected.Count) { throw 'Checksums are missing release files.' }

$executable = Join-Path $dist 'stat-wisp.exe'
$info = (Get-Item -LiteralPath $executable).VersionInfo
if ($info.FileVersion -ne $Version -or $info.ProductVersion -ne $Version -or $info.ProductName -ne 'Stat Wisp') {
    throw 'Executable version information does not match the release.'
}
$bytes = [IO.File]::ReadAllBytes($executable)
$peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
if ([BitConverter]::ToUInt32($bytes, $peOffset) -ne 0x4550 -or
    [BitConverter]::ToUInt16($bytes, $peOffset + 4) -ne 0x8664) {
    throw 'Release executable must be a Windows x64 PE image.'
}
# IMAGE_DLLCHARACTERISTICS: high-entropy ASLR, ASLR, DEP, and Control Flow Guard.
$dllCharacteristics = [BitConverter]::ToUInt16($bytes, $peOffset + 24 + 70)
if (($dllCharacteristics -band 0x4160) -ne 0x4160) { throw 'Executable is missing required memory protections.' }
$installer = Join-Path $dist "stat-wisp-$Version-setup.exe"
if ((Get-Item -LiteralPath $installer).VersionInfo.FileVersion.Trim() -ne "$Version.0") {
    throw 'Installer version does not match the release.'
}
if ((Get-FileHash -LiteralPath (Join-Path $dist 'LICENSE.txt')).Hash -ne
    (Get-FileHash -LiteralPath (Join-Path $repository 'LICENSE')).Hash) {
    throw 'Packaged license does not match the repository license.'
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $dist "stat-wisp-$Version-win-x64.zip"))
try {
    if (Compare-Object @('LICENSE.txt', 'README.txt', 'stat-wisp.exe') @($archive.Entries.FullName | Sort-Object)) {
        throw 'Portable ZIP has missing or unexpected contents.'
    }
    foreach ($name in @('LICENSE.txt', 'stat-wisp.exe')) {
        $stream = $archive.GetEntry($name).Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try {
            $digest = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '')
            if ($digest -ne (Get-FileHash -LiteralPath (Join-Path $dist $name)).Hash) {
                throw "Portable ZIP contains a different $name."
            }
        } finally { $sha.Dispose(); $stream.Dispose() }
    }
} finally { $archive.Dispose() }
Write-Host 'Package checks passed: exact assets, SHA-256, versions, x64, memory protections, ZIP contents, and license.'

if (-not $Install) {
    if ($PreviousInstaller) { throw '-PreviousInstaller requires -Install.' }
    return
}
if ($PreviousInstaller) {
    $PreviousInstaller = (Resolve-Path -LiteralPath $PreviousInstaller).Path
    $previousVersion = (Get-Item -LiteralPath $PreviousInstaller).VersionInfo.ProductVersion.Trim()
    if ($previousVersion -notmatch '^\d+\.\d+\.\d+$') { throw 'Previous installer has no valid product version.' }
}

# Refuse to overwrite an existing installation or startup preference on developer machines.
$uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{96D1C574-E951-4E2D-AC31-BBDBB1B5915F}_is1'
$machineKey = $uninstallKey.Replace('HKCU:', 'HKLM:')
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
if ((Test-Path -LiteralPath $uninstallKey) -or (Test-Path -LiteralPath $machineKey)) {
    throw 'Installer smoke test requires a machine without an existing Stat Wisp installation.'
}
if ((Get-ItemProperty -LiteralPath $runKey -ErrorAction SilentlyContinue).PSObject.Properties.Name -contains 'stat-wisp') {
    throw 'Installer smoke test requires no existing Stat Wisp startup entry.'
}
if (Get-Process -Name stat-wisp -ErrorAction SilentlyContinue) { throw 'Exit Stat Wisp before the installer smoke test.' }

$testRoot = [IO.Path]::GetFullPath((Join-Path $repository ('out\package-test-' + [guid]::NewGuid().ToString('N'))))
if (-not $testRoot.StartsWith($repository + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected package-test path: $testRoot"
}
New-Item -ItemType Directory -Path $testRoot | Out-Null
$installDirectory = Join-Path $testRoot 'installed'
$uninstaller = Join-Path $installDirectory 'unins000.exe'
function Invoke-Setup([string]$File, [string[]]$Arguments) {
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    try {
        if (-not $process.WaitForExit(60000)) {
            Stop-Process -Id $process.Id -Force
            throw 'Installer operation timed out; inspect the package-test logs.'
        }
        if ($process.ExitCode -ne 0) { throw "Installer operation failed with exit code $($process.ExitCode)." }
    } finally { $process.Dispose() }
}
try {
    if ($PreviousInstaller) {
        Invoke-Setup $PreviousInstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/NOICONS',
            ('/DIR="' + $installDirectory + '"'), ('/LOG="' + (Join-Path $testRoot 'previous-install.log') + '"'))
        if ((Get-Item -LiteralPath (Join-Path $installDirectory 'stat-wisp.exe')).VersionInfo.FileVersion -ne $previousVersion) {
            throw 'Previous installer did not install the expected application version.'
        }
    }
    foreach ($pass in @('install', 'repair')) {
        Invoke-Setup $installer @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/NOICONS',
            ('/DIR="' + $installDirectory + '"'), ('/LOG="' + (Join-Path $testRoot "$pass.log") + '"'))
        $installedExe = Join-Path $installDirectory 'stat-wisp.exe'
        if ((Get-FileHash -LiteralPath $installedExe).Hash -ne (Get-FileHash -LiteralPath $executable).Hash) {
            throw 'Installed executable differs from the verified release.'
        }
        $registration = Get-ItemProperty -LiteralPath $uninstallKey
        if ($registration.DisplayVersion -ne $Version -or
            $registration.InstallLocation.TrimEnd('\') -ne $installDirectory) {
            throw 'Per-user installation registration is incorrect.'
        }
    }
    & (Join-Path $PSScriptRoot 'test-exit.ps1') -Executable $installedExe -TestDirectory (Join-Path $testRoot 'runtime')
} finally {
    if (Test-Path -LiteralPath $uninstaller) {
        Invoke-Setup $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART',
            ('/LOG="' + (Join-Path $testRoot 'uninstall.log') + '"'))
    }
}
if ((Test-Path -LiteralPath (Join-Path $installDirectory 'stat-wisp.exe')) -or (Test-Path -LiteralPath $uninstallKey)) {
    throw 'Uninstall did not remove the executable and installation registration.'
}
Write-Host "Installer checks passed: per-user install, repair, application Exit, and silent uninstall. Logs: $testRoot"
if ($PreviousInstaller) { Write-Host "Upgrade from $previousVersion to $Version passed." }
