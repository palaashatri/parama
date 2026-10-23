#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$IsoPath = 'build/parama.iso',
    [string]$VmName = 'PARAMA-QA',
    [ValidateRange(5, 300)][int]$TimeoutSeconds = 50,
    [ValidateSet('BOOT')][string]$TargetMilestone = 'BOOT',
    [string]$VBoxManagePath = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildDir = Join-Path $repo 'build'
$isoCandidate = if ([System.IO.Path]::IsPathRooted($IsoPath)) { $IsoPath } else { Join-Path $repo $IsoPath }
$iso = (Resolve-Path $isoCandidate -ErrorAction Stop).Path
$logFile = Join-Path $buildDir 'com1.log'
$screenshot = Join-Path $buildDir 'qa_screen.png'
$marker = 'org.parama.qa'
$started = $false
$serialPass = $false
$screenPass = $false
$runError = $null

if (-not (Test-Path -LiteralPath $VBoxManagePath)) { throw "VBoxManage not found: $VBoxManagePath" }
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
function VBox([string[]]$CommandArgs) {
    $output = & $VBoxManagePath @CommandArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "VBoxManage $($CommandArgs -join ' ') failed (exit $LASTEXITCODE): $($output -join ' ')"
    }
    return $output
}

# Never take control of someone else's pre-existing VM.
$vms = @(VBox @('list', 'vms'))
$exists = @($vms | Where-Object { $_ -match ('^"' + [regex]::Escape($VmName) + '"\s+\{') }).Count -gt 0
if (-not $exists) {
    $basefolder = Join-Path $buildDir 'vbox'
    New-Item -ItemType Directory -Force -Path $basefolder | Out-Null
    VBox @('createvm', '--name', $VmName, '--ostype', 'Other_64', '--basefolder', $basefolder, '--register') | Out-Null
    VBox @('setextradata', $VmName, $marker, 'owned') | Out-Null
    VBox @('storagectl', $VmName, '--name', 'IDE Controller', '--add', 'ide') | Out-Null
}
$owner = (VBox @('getextradata', $VmName, $marker)) -join ' '
if ($owner -notmatch 'Value:\s*owned') { throw "VM '$VmName' is not tagged as a PARAMA QA-owned VM. Refusing to modify it." }
$info = (VBox @('showvminfo', $VmName, '--machinereadable')) -join "`n"
if ($info -notmatch 'VMState="poweroff"') { throw "VM '$VmName' is not powered off; refusing to interfere." }

Remove-Item -LiteralPath $logFile, $screenshot -ErrorAction SilentlyContinue
VBox @('modifyvm', $VmName, '--memory', '512', '--vram', '32', '--cpus', '1',
       '--firmware', 'efi', '--graphicscontroller', 'vmsvga', '--mouse', 'ps2', '--keyboard', 'ps2',
       '--uart', '1', '0x3f8', '4', '--uart-mode', '1', 'file', $logFile,
       '--nic', '1=none', '--audio-enabled', 'off', '--boot', '1=dvd') | Out-Null
VBox @('storageattach', $VmName, '--storagectl', 'IDE Controller', '--port', '1', '--device', '0',
       '--type', 'dvddrive', '--medium', $iso) | Out-Null
try {
    VBox @('startvm', $VmName, '--type', 'headless') | Out-Null
    $started = $true
    Write-Host "[QA] Booting '$VmName' from '$iso'"
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    while ($clock.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        Start-Sleep -Milliseconds 500
        if (Test-Path -LiteralPath $logFile) {
            $logContent = Get-Content -LiteralPath $logFile -Raw -ErrorAction SilentlyContinue
            if ($logContent -and $logContent.Contains('[BRAHMA] BOOT_OK') -and
                $logContent.Contains('[BHUMI] Boot frame smoke test passed') -and
                $logContent.Contains('[BRAHMA] Boot diagnostic framebuffer drawn') -and
                -not $logContent.Contains('[BRAHMA][FATAL]')) {
                $serialPass = $true
                break
            }
            if ($logContent -and $logContent.Contains('[BRAHMA][FATAL]')) { break }
        }
    }
    $clock.Stop()
} catch {
    $runError = $_
} finally {
    if ($started) {
        # Capture evidence on failure as well as success; never infer rendered pixels from serial alone.
        try { VBox @('controlvm', $VmName, 'screenshotpng', $screenshot) | Out-Null } catch {
            Write-Warning "Screenshot failed: $_"
        }
        try { VBox @('controlvm', $VmName, 'poweroff') | Out-Null } catch {
            Write-Warning "Power-off failed: $_"
        }
    }
}
# A PNG header check is intentionally weaker than image content/interaction verification.
if (Test-Path -LiteralPath $screenshot) {
    $bytes = [System.IO.File]::ReadAllBytes($screenshot)
    $screenPass = $bytes.Length -gt 1000 -and $bytes[0] -eq 137 -and $bytes[1] -eq 80 -and
                  $bytes[2] -eq 78 -and $bytes[3] -eq 71
}
if ($runError -or -not $serialPass -or -not $screenPass) {
    Write-Host '[QA] Captured serial log:'
    if (Test-Path -LiteralPath $logFile) { Get-Content -LiteralPath $logFile }
    if ($runError) { Write-Error $runError }
    throw "[QA][FAIL] $TargetMilestone serial=$serialPass screenshot=$screenPass. Evidence: $buildDir"
}
Write-Host "[QA][PASS] $TargetMilestone: serial milestone + PNG capture"
Write-Host "[QA] Serial: $logFile"
Write-Host "[QA] Screenshot: $screenshot (manual visual review still required)"
