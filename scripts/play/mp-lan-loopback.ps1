# Two-process LAN loopback test of the multiplayer shell: a hosting game and
# a joining game on this PC talk over UDP (LAN discovery on 50010, the lobby
# on 50020) and must agree on the session (seed + options string).
#   pwsh scripts/play/mp-lan-loopback.ps1 [-Exe build/ra2engine.exe] [-Out build/play-out]
# Needs RA2_INSTALL_DIR. Each process loads the shell art (~15 s).
[CmdletBinding()]
param([string]$Exe = "build/ra2engine.exe", [string]$Out = "build/play-out")
$ErrorActionPreference = "Stop"
$hostOut = Join-Path $Out "mp-lan-host"; $joinOut = Join-Path $Out "mp-lan-join"
New-Item -ItemType Directory -Force $hostOut, $joinOut | Out-Null
$hostLog = Join-Path $hostOut "stdout.txt"; $joinLog = Join-Path $joinOut "stdout.txt"
$h = Start-Process -FilePath $Exe -ArgumentList @("--play", "scripts/play/mp-lan-host.play", "--out", $hostOut) `
     -RedirectStandardOutput $hostLog -RedirectStandardError (Join-Path $hostOut "stderr.txt") -PassThru -NoNewWindow
$deadline = (Get-Date).AddSeconds(120)
while ((Get-Date) -lt $deadline -and -not (Select-String -Path (Join-Path $hostOut "stderr.txt") -Pattern "hosting LAN game" -Quiet -ErrorAction SilentlyContinue)) { Start-Sleep -Milliseconds 500 }
$j = Start-Process -FilePath $Exe -ArgumentList @("--play", "scripts/play/mp-lan-join.play", "--out", $joinOut) `
     -RedirectStandardOutput $joinLog -RedirectStandardError (Join-Path $joinOut "stderr.txt") -PassThru -NoNewWindow
$ok = $j.WaitForExit(300000) -and $h.WaitForExit(120000)
if (-not $ok) { foreach ($p in @($h, $j)) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } } }
$hs = Select-String -Path (Join-Path $hostOut "stderr.txt") -Pattern "\[mp\] session seed=(\d+)" | Select-Object -First 1
$js = Select-String -Path (Join-Path $joinOut "stderr.txt") -Pattern "\[mp\] session seed=(\d+)" | Select-Object -First 1
$ho = Select-String -Path (Join-Path $hostOut "stderr.txt") -Pattern "\[mp\]   options: (.*)" | Select-Object -First 1
$jo = Select-String -Path (Join-Path $joinOut "stderr.txt") -Pattern "\[mp\]   options: (.*)" | Select-Object -First 1
$hr = Get-Content (Join-Path $hostOut "result.txt") -TotalCount 1 -ErrorAction SilentlyContinue
$jr = Get-Content (Join-Path $joinOut "result.txt") -TotalCount 1 -ErrorAction SilentlyContinue
Write-Host "host: $hr (exit $($h.ExitCode))"; Write-Host "join: $jr (exit $($j.ExitCode))"
$same = $hs -and $js -and $hs.Matches[0].Groups[1].Value -eq $js.Matches[0].Groups[1].Value -and $ho.Matches[0].Groups[1].Value -eq $jo.Matches[0].Groups[1].Value
Write-Host ("session agreement (seed + options string): " + $(if ($same) { "SAME" } else { "DIFFERENT / missing" }))
if ($h.ExitCode -ne 0 -or $j.ExitCode -ne 0 -or -not $same) { exit 1 }
exit 0
