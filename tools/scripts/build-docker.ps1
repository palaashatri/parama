#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not (Get-Command docker.exe -ErrorAction SilentlyContinue)) {
    throw 'Docker Desktop CLI not found. Start/install Docker Desktop, or build using Ubuntu WSL2.'
}
Push-Location $repo
try {
    Write-Host '[BUILD] Creating the Linux cross-toolchain container...'
    & docker.exe build -f toolchain/Dockerfile -t parama-toolchain:0.1.0 .
    if ($LASTEXITCODE -ne 0) { throw "Docker toolchain build failed ($LASTEXITCODE)" }
    Write-Host '[BUILD] Building BRAHMA and Limine ISO in Docker Desktop...'
    & docker.exe run --rm --mount "type=bind,source=$repo,target=/work" parama-toolchain:0.1.0 ./tools/scripts/build.sh
    if ($LASTEXITCODE -ne 0) { throw "Docker PARAMA build failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
Write-Host "[BUILD] ISO created: $repo\build\parama.iso"
