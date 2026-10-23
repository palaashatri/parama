#requires -Version 7.0
[CmdletBinding()]
param([string]$Distro = 'Ubuntu')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw 'WSL2 not found. Install Ubuntu WSL2 or use build-docker.ps1 with Docker Desktop.'
}
Write-Host '[BUILD] Compiling/packaging in WSL2; VirtualBox runs on Windows.'
& wsl.exe --distribution $Distro --cd $repo --exec bash ./tools/scripts/build.sh
if ($LASTEXITCODE -ne 0) { throw "WSL build failed (exit $LASTEXITCODE). Check the distro name; use -Distro Ubuntu-24.04 if needed, or build-docker.ps1." }
Write-Host "[BUILD] Result: $repo\build\parama.iso"
