# Two-process wave-9 menus test (docs/gap/wave9/03-menus-vs-original.md): the
# Network Lobby page (Players + lobby chat), Host / Join Screen, the edit burst
# after a join (W9-01-08), a Megawealth match, Diplomacy names, the MP options
# menu and the MP score page. Both halves start together: they meet on the
# Network page first (LAN discovery on 50010), then the lobby runs on 50020.
#   pwsh scripts/play/mp-menus-pair.ps1 [-Exe build/ra2engine.exe] [-Out build/play-out]
# Needs RA2_INSTALL_DIR. Take the network lock first (w9fix-COMMON.md).
[CmdletBinding()]
param([string]$Exe = "build/ra2engine.exe", [string]$Out = "build/play-out")
$ErrorActionPreference = "Stop"
$hostOut = Join-Path $Out "mp-menus-host"; $joinOut = Join-Path $Out "mp-menus-join"
New-Item -ItemType Directory -Force $hostOut, $joinOut | Out-Null
$h = Start-Process -FilePath $Exe -ArgumentList @("--play", "scripts/play/mp-menus-host.play", "--out", $hostOut) `
     -RedirectStandardOutput (Join-Path $hostOut "stdout.txt") -RedirectStandardError (Join-Path $hostOut "stderr.txt") -PassThru -NoNewWindow
Start-Sleep -Seconds 2
$j = Start-Process -FilePath $Exe -ArgumentList @("--play", "scripts/play/mp-menus-join.play", "--out", $joinOut) `
     -RedirectStandardOutput (Join-Path $joinOut "stdout.txt") -RedirectStandardError (Join-Path $joinOut "stderr.txt") -PassThru -NoNewWindow
$ok = $j.WaitForExit(540000) -and $h.WaitForExit(60000)
if (-not $ok) { foreach ($p in @($h, $j)) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } } }
$hs = Select-String -Path (Join-Path $hostOut "stderr.txt") -Pattern "\[mp\] session seed=(\d+)" | Select-Object -First 1
$js = Select-String -Path (Join-Path $joinOut "stderr.txt") -Pattern "\[mp\] session seed=(\d+)" | Select-Object -First 1
$hr = Get-Content (Join-Path $hostOut "result.txt") -TotalCount 1 -ErrorAction SilentlyContinue
$jr = Get-Content (Join-Path $joinOut "result.txt") -TotalCount 1 -ErrorAction SilentlyContinue
Write-Host "host: $hr (exit $($h.ExitCode))"; Write-Host "join: $jr (exit $($j.ExitCode))"
$same = $hs -and $js -and $hs.Matches[0].Groups[1].Value -eq $js.Matches[0].Groups[1].Value
Write-Host ("session seed agreement: " + $(if ($same) { "SAME" } else { "DIFFERENT / missing" }))
if ($h.ExitCode -ne 0 -or $j.ExitCode -ne 0 -or -not $same) { exit 1 }
exit 0
