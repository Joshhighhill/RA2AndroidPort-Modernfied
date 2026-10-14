# W9-05-01: "no UI action changes the simulation without a command".
# Plays a UI-driven script with --record (RA2_REC_CHECK: a state-hash
# checkpoint every 15 ticks), then --replays the command log with NO UI: the
# replay re-applies only the recorded commands, so any UI path that mutated
# the World directly (the touch cameo-menu Cancel did) makes a checkpoint or
# the end state differ ("[replay] ... match=no"). Single process, no network.
#   pwsh scripts/play/ui-replay.ps1 [-Exe build/ra2engine.exe] [-Out build/play-out]
#        [-Scripts ui-replay-touch.play,...]
# Needs RA2_INSTALL_DIR.
[CmdletBinding()]
param([string]$Exe = "build/ra2engine.exe", [string]$Out = "build/play-out",
      [string[]]$Scripts = @("ui-replay-touch.play"))
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $Out | Out-Null
$all = $true
foreach ($s in $Scripts) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($s)
    $dir = Join-Path $Out "ui-replay-$name"
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    New-Item -ItemType Directory -Force $dir | Out-Null
    $log = Join-Path (Resolve-Path $dir) "rec.ra2cmd"
    $env:RA2_REC_CHECK = "15"
    $p = Start-Process -FilePath $Exe -ArgumentList @("--play", "scripts/play/$s", "--out", $dir, "--record", $log) `
         -RedirectStandardOutput (Join-Path $dir "rec-stdout.txt") -RedirectStandardError (Join-Path $dir "rec-stderr.txt") -PassThru -NoNewWindow -Wait
    $recOk = $p.ExitCode -eq 0 -and (Test-Path $log)
    $r = Start-Process -FilePath $Exe -ArgumentList @("--replay", $log) `
         -RedirectStandardOutput (Join-Path $dir "rep-stdout.txt") -RedirectStandardError (Join-Path $dir "rep-stderr.txt") -PassThru -NoNewWindow -Wait
    Remove-Item Env:RA2_REC_CHECK -ErrorAction SilentlyContinue
    $line = Select-String -Path (Join-Path $dir "rep-stderr.txt") -Pattern "\[replay\] end .*" -ErrorAction SilentlyContinue | Select-Object -Last 1
    $div = Select-String -Path (Join-Path $dir "rep-stderr.txt") -Pattern "\[replay\] first divergence.*" -ErrorAction SilentlyContinue | Select-Object -First 1
    $ok = $recOk -and $r.ExitCode -eq 0 -and $line -and ($line.Matches[0].Value -match "match=yes") -and -not $div
    Write-Host ("{0}: record exit {1}, replay exit {2}: {3}" -f $s, $p.ExitCode, $r.ExitCode, $(if ($line) { $line.Matches[0].Value } else { "no replay result" }))
    if ($div) { Write-Host ("  " + $div.Matches[0].Value) }
    Write-Host ("  -> " + $(if ($ok) { "PASS (every UI action went through a command)" } else { "FAIL" }))
    if (-not $ok) { $all = $false }
}
if (-not $all) { exit 1 }
exit 0
