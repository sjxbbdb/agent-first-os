[CmdletBinding()]
param(
    [switch]$Run
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$wslRoot = (wsl.exe wslpath -a $repoRoot).Trim()

if ([string]::IsNullOrWhiteSpace($wslRoot)) {
    throw "Unable to map repository path to WSL: $repoRoot"
}

wsl.exe bash "$wslRoot/product/tools/build-bios.sh"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($Run) {
    wsl.exe bash "$wslRoot/product/tools/run-bios.sh"
    exit $LASTEXITCODE
}
