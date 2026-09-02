param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = '0.3.0'
)

& (Join-Path $PSScriptRoot 'build-release.ps1') -Version $Version
exit $LASTEXITCODE
