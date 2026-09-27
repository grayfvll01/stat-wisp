param(
    [ValidatePattern('^(\d+\.\d+\.\d+)?$')]
    [string]$Version = '',
    [switch]$ExeOnly
)

$ErrorActionPreference = 'Stop'
$repository = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = Join-Path $repository 'out\stat-wisp-release'
$dist = Join-Path $repository 'dist'
$portable = Join-Path $dist 'portable'

function Find-CMake {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($installation) {
            $bundled = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $bundled) { return $bundled }
        }
    }
    throw 'CMake was not found. Install Visual Studio Desktop development with C++ or add CMake to PATH.'
}

function Find-Iscc {
    $command = Get-Command iscc.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
    )
    return $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

# VERSION is the single source of the release version.
$sourceVersion = (Get-Content -LiteralPath (Join-Path $repository 'VERSION') -TotalCount 1).Trim()
if ($sourceVersion -notmatch '^\d+\.\d+\.\d+$') { throw 'VERSION must contain MAJOR.MINOR.PATCH.' }
if (-not $Version) { $Version = $sourceVersion }
if ($Version -ne $sourceVersion) { throw "Requested version $Version does not match VERSION ($sourceVersion)." }

$cmake = Find-CMake
$iscc = if (-not $ExeOnly) { Find-Iscc }
if (-not $ExeOnly -and -not $iscc) { throw 'Install Inno Setup 6 to build the required installer.' }
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
Write-Host "Building Stat Wisp $Version with $cmake"

& $cmake -S $repository -B $build -A x64 -DCMAKE_CONFIGURATION_TYPES=Release -DBUILD_TESTING=ON -DSTAT_WISP_BUILD_TESTS=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmake --build $build --config Release --clean-first --parallel
if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
& $ctest --test-dir $build -C Release --output-on-failure --no-tests=error
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }

if (Test-Path -LiteralPath $dist) {
    $resolvedDist = [System.IO.Path]::GetFullPath($dist)
    if (-not $resolvedDist.StartsWith($repository + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean unexpected artifact path: $resolvedDist"
    }
    Remove-Item -LiteralPath $resolvedDist -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $dist | Out-Null

$executable = Join-Path $build 'Release\stat-wisp.exe'
if (-not (Test-Path -LiteralPath $executable)) { throw "Build did not produce $executable" }
$binaryVersion = (Get-Item -LiteralPath $executable).VersionInfo
if ($binaryVersion.FileVersion -ne $Version -or $binaryVersion.ProductName -ne 'Stat Wisp') {
    throw 'Executable version or product name is incorrect.'
}
Copy-Item -LiteralPath $executable -Destination (Join-Path $dist 'stat-wisp.exe')
Copy-Item -LiteralPath (Join-Path $repository 'LICENSE') -Destination (Join-Path $dist 'LICENSE.txt')

if (-not $ExeOnly) {
    New-Item -ItemType Directory -Force -Path $portable | Out-Null
    Copy-Item -LiteralPath $executable -Destination (Join-Path $portable 'stat-wisp.exe')
    Copy-Item -LiteralPath (Join-Path $repository 'LICENSE') -Destination (Join-Path $portable 'LICENSE.txt')

    @"
Stat Wisp $Version

Your PC's vitals, quietly in the Windows tray.

Run stat-wisp.exe and select at least one metric. Right-click any tray icon
for settings, updates, pause, or exit. Settings are stored in
%LOCALAPPDATA%\stat-wisp\settings.ini.

Unsupported sensor values are shown as --.

https://github.com/grayfvll01/stat-wisp
"@ | Set-Content -LiteralPath (Join-Path $portable 'README.txt') -Encoding utf8

    $zip = Join-Path $dist "stat-wisp-$Version-win-x64.zip"
    Compress-Archive -Path (Join-Path $portable '*') -DestinationPath $zip -CompressionLevel Optimal
    Remove-Item -LiteralPath $portable -Recurse -Force

    Write-Host "Using Inno Setup: $iscc"
    & $iscc "/DAppVersion=$Version" (Join-Path $repository 'installer\stat-wisp.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Installer build failed.' }

    # In-app updates read this manifest from the latest published release.
    $installer = Get-Item -LiteralPath (Join-Path $dist "stat-wisp-$Version-setup.exe")
    $standalone = Get-Item -LiteralPath (Join-Path $dist 'stat-wisp.exe')
    $manifest = @(
        'format=1'
        "version=$Version"
        "installer=$($installer.Name)"
        "installer_sha256=$((Get-FileHash -LiteralPath $installer.FullName -Algorithm SHA256).Hash.ToLowerInvariant())"
        "installer_size=$($installer.Length)"
        "portable=$($standalone.Name)"
        "portable_sha256=$((Get-FileHash -LiteralPath $standalone.FullName -Algorithm SHA256).Hash.ToLowerInvariant())"
        "portable_size=$($standalone.Length)"
    ) -join "`n"
    [IO.File]::WriteAllText((Join-Path $dist 'stat-wisp-update.ini'), "$manifest`n", [Text.Encoding]::ASCII)
}

$releaseFiles = Get-ChildItem -LiteralPath $dist -File | Where-Object { $_.Name -ne 'SHA256SUMS.txt' } | Sort-Object Name
$checksumLines = foreach ($file in $releaseFiles) {
    $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant())  $($file.Name)"
}
$checksumLines | Set-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.txt') -Encoding ascii

if (-not $ExeOnly) {
    & (Join-Path $PSScriptRoot 'test-release.ps1') -Version $Version -DistDirectory $dist
}

Write-Host 'Release artifacts:'
Get-ChildItem -LiteralPath $dist -File | ForEach-Object { Write-Host "  $($_.FullName)" }
