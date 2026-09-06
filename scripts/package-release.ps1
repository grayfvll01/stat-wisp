param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = '0.4.0'
)

& (Join-Path $PSScriptRoot 'build-release.ps1') -Version $Version
exit $LASTEXITCODE
