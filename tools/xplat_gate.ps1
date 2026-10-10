# Net L0 cross-platform determinism gate.
#
# Runs the --demo-* gates on the desktop build AND on an Android device /
# emulator (the RA2_ANDROID_CLI executable, see README.md "Networking L0")
# and compares every hash / witness count they print. Lockstep multiplayer
# needs these to be identical across compilers (MinGW gcc vs NDK clang),
# C libraries (UCRT vs bionic) and CPUs.
#
#   pwsh tools/xplat_gate.ps1 -Exe C:\ra2-build\net-l0\ra2engine.exe `
#        -AndroidExe C:\ra2-build\net-l0-android\ra2engine -Out <dir> [-Gates --demo-base,...] [-SkipPush]
#
# Device side: the game files must already be in $RemoteData (adb push them
# once; the script pushes only the executable). Exit code 0 = every gate
# matched.
param(
    [Parameter(Mandatory)][string]$Exe,
    [Parameter(Mandatory)][string]$AndroidExe,
    [Parameter(Mandatory)][string]$Out,
    [string[]]$Gates,
    [string]$RemoteDir = "/data/local/tmp/l0",
    [string]$DesktopData = "D:\Red Alert 2",
    [switch]$SkipPush,
    # Also run `--ai-soak --ai-vs-ai --minutes N --seed $SoakSeed` on both and
    # compare fnv1a(save_world) every 15 frames, the RESULT line and the final
    # state_hash (0 = skip).
    [int]$SoakMinutes = 0,
    [string]$SoakSeed = "0x4E4C3030"
)
$ErrorActionPreference = "Stop"
if (-not $Gates) {
    $Gates = "--demo-combat","--demo-tank","--demo-base","--demo-econ","--demo-prod","--demo-transport",
             "--demo-aircraft","--demo-naval","--demo-superweapon","--demo-techtree","--demo-capture",
             "--demo-shroud","--demo-multihouse","--demo-skirmish","--demo-roster","--demo-roster2",
             "--demo-abilities","--demo-repair","--demo-projectile","--demo-triggers","--demo-veterancy",
             "--demo-maps","--demo-campaign","--demo-saveload","--demo-ai-base","--demo-ai-team","--demo-ai-trigger"
}
New-Item -ItemType Directory -Force $Out | Out-Null
if (-not $SkipPush) {
    adb shell mkdir -p "$RemoteDir/ra2" | Out-Null
    adb push $AndroidExe "$RemoteDir/ra2engine" | Out-Null
    adb shell chmod 755 "$RemoteDir/ra2engine" | Out-Null
}
# The values that must match: every *hash=<16 hex>, logs=/lines= counts and
# identical=/deterministic= verdicts (screenshot paths and timings excluded).
function Get-Witness([string]$path) {
    $text = Get-Content $path -Raw
    $m = [regex]::Matches($text, '(\w*hash=[0-9a-f]{16}|\blogs=\d+|\blines=\d+|\bidentical=\w+|\bdeterministic=\w+)')
    ($m | ForEach-Object Value) -join ' '
}
$env:RA2_INSTALL_DIR = $DesktopData
$summary = Join-Path $Out "xplat.txt"
Set-Content $summary "# xplat gate: desktop=$Exe android=$AndroidExe"
$fail = 0
Push-Location $Out
foreach ($g in $Gates) {
    $name = $g.TrimStart('-')
    $dlog = Join-Path $Out "$name.desktop.log"
    $alog = Join-Path $Out "$name.android.log"
    & $Exe $g *> $dlog
    $drc = $LASTEXITCODE
    adb shell "cd $RemoteDir && SDL_RENDER_DRIVER=software SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RA2_PREF_DIR=$RemoteDir/ RA2_INSTALL_DIR=$RemoteDir/ra2/ ./ra2engine $g; echo ANDROID_RC=`$?" *> $alog
    $arc = ((Select-String -Path $alog -Pattern 'ANDROID_RC=(\d+)' | Select-Object -Last 1).Matches.Groups[1].Value)
    $dw = Get-Witness $dlog
    $aw = Get-Witness $alog
    $ok = ($dw -eq $aw) -and ($dw -ne "") -and ("$drc" -eq "$arc")
    if (-not $ok) { $fail++ }
    $line = "{0} {1} rc={2}/{3} desktop=[{4}]" -f ($(if ($ok) { "MATCH" } else { "DIFF " })), $g, $drc, $arc, $dw
    if (-not $ok) { $line += " android=[$aw]" }
    Add-Content $summary $line
    Write-Host $line
}
if ($SoakMinutes -gt 0) {
    $sargs = "--ai-soak --ai-vs-ai --minutes $SoakMinutes --seed $SoakSeed --hash-every 15"
    $dlog = Join-Path $Out "ai-soak.desktop.log"
    $alog = Join-Path $Out "ai-soak.android.log"
    & $Exe @($sargs -split ' ') *> $dlog
    adb shell "cd $RemoteDir && SDL_RENDER_DRIVER=software SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RA2_PREF_DIR=$RemoteDir/ RA2_INSTALL_DIR=$RemoteDir/ra2/ ./ra2engine $sargs" *> $alog
    # fnv1a(save_world) every 15 frames -- the lockstep match's sync hash basis
    # (GameScene::net_state_hash) -- plus the RESULT line and the final state.
    $pick = { param($f) ((Select-String -Path $f -Pattern '\[ai-soak\] (RESULT|state_hash|hash f=)').Line) -join ' | ' }
    $dall = & $pick $dlog
    $aall = & $pick $alog
    $nh = (Select-String -Path $dlog -Pattern '\[ai-soak\] hash f=').Count
    $ok = ($dall -eq $aall) -and ($dall -match 'state_hash=') -and ($nh -gt 0)
    $dw = "periodic_hashes=$nh " + (((Select-String -Path $dlog -Pattern '\[ai-soak\] (RESULT|state_hash)').Line) -join ' | ')
    $aw = "periodic_hashes=" + (Select-String -Path $alog -Pattern '\[ai-soak\] hash f=').Count
    if (-not $ok) { $fail++ }
    $line = "{0} ai-soak {1} desktop=[{2}]" -f ($(if ($ok) { "MATCH" } else { "DIFF " })), $sargs, $dw
    if (-not $ok) { $line += " android=[$aw]" }
    Add-Content $summary $line
    Write-Host $line
}
Pop-Location
Add-Content $summary "RESULT $(if ($fail -eq 0) { 'PASS' } else { "FAIL ($fail)" })"
exit $fail
