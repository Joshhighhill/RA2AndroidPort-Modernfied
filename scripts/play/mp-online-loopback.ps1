# Online loopback test: ra2lobbyd (built from server/ if -Server is not
# given) on 127.0.0.1 with --allow-loopback, a hosting game and a joining
# game. The join goes through the room list + the CnCNet V3 tunnel relay;
# both games must agree on the session (seed + options string).
#   pwsh scripts/play/mp-online-loopback.ps1 [-Exe build/ra2engine.exe] [-Out build/play-out] [-Server path\ra2lobbyd.exe]
[CmdletBinding()]
param([string]$Exe = "build/ra2engine.exe", [string]$Out = "build/play-out", [string]$Server = "")
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $Out | Out-Null
if (-not $Server) {
    $Server = Join-Path (Resolve-Path $Out) "ra2lobbyd.exe"
    Push-Location server; go build -o $Server ./cmd/ra2lobbyd; Pop-Location
}
$srvLog = Join-Path $Out "ra2lobbyd.log"
$srv = Start-Process -FilePath $Server -ArgumentList @("--allow-loopback", "--server-name", "`"Loopback Test`"", "--relay-ports", "50100-50109", "--admin", "127.0.0.1:18080") `
       -RedirectStandardError $srvLog -RedirectStandardOutput (Join-Path $Out "ra2lobbyd.out") -PassThru -NoNewWindow
Start-Sleep -Seconds 1
$hostOut = Join-Path $Out "mp-online-host"; $joinOut = Join-Path $Out "mp-online-join"
New-Item -ItemType Directory -Force $hostOut, $joinOut | Out-Null
$h = Start-Process -FilePath $Exe -ArgumentList @("--play", "scripts/play/mp-online-host.play", "--out", $hostOut) `
     -RedirectStandardOutput (Join-Path $hostOut "stdout.txt") -RedirectStandardError (Join-Path $hostOut "stderr.txt") -PassThru -NoNewWindow
$deadline = (Get-Date).AddSeconds(120)
while ((Get-Date) -lt $deadline -and -not (Select-String -Path (Join-Path $hostOut "stderr.txt") -Pattern "room created code=" -Quiet -ErrorAction SilentlyContinue)) { Start-Sleep -Milliseconds 500 }
$j = Start-Process -FilePath $Exe -ArgumentList @("--play", "scripts/play/mp-online-join.play", "--out", $joinOut) `
     -RedirectStandardOutput (Join-Path $joinOut "stdout.txt") -RedirectStandardError (Join-Path $joinOut "stderr.txt") -PassThru -NoNewWindow
$ok = $j.WaitForExit(300000) -and $h.WaitForExit(120000)
foreach ($p in @($h, $j)) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
$metrics = try { (Invoke-WebRequest -UseBasicParsing http://127.0.0.1:18080/metrics -TimeoutSec 3).Content } catch { "" }
Stop-Process -Id $srv.Id -Force
$fwd = if ($metrics -match "ra2lobbyd_v3_forwarded_total (\d+)") { [int]$Matches[1] } else { -1 }
$hs = Select-String -Path (Join-Path $hostOut "stderr.txt") -Pattern "\[mp\] session seed=(\d+)" | Select-Object -First 1
$js = Select-String -Path (Join-Path $joinOut "stderr.txt") -Pattern "\[mp\] session seed=(\d+)" | Select-Object -First 1
$ho = Select-String -Path (Join-Path $hostOut "stderr.txt") -Pattern "\[mp\]   options: (.*)" | Select-Object -First 1
$jo = Select-String -Path (Join-Path $joinOut "stderr.txt") -Pattern "\[mp\]   options: (.*)" | Select-Object -First 1
Write-Host ("host: " + (Get-Content (Join-Path $hostOut "result.txt") -TotalCount 1 -ErrorAction SilentlyContinue) + " (exit $($h.ExitCode))")
Write-Host ("join: " + (Get-Content (Join-Path $joinOut "result.txt") -TotalCount 1 -ErrorAction SilentlyContinue) + " (exit $($j.ExitCode))")
Write-Host "V3 tunnel datagrams forwarded by ra2lobbyd: $fwd"
$same = $hs -and $js -and $hs.Matches[0].Groups[1].Value -eq $js.Matches[0].Groups[1].Value -and $ho.Matches[0].Groups[1].Value -eq $jo.Matches[0].Groups[1].Value
Write-Host ("session agreement (seed + options string): " + $(if ($same) { "SAME" } else { "DIFFERENT / missing" }))
if ($h.ExitCode -ne 0 -or $j.ExitCode -ne 0 -or -not $same -or $fwd -le 0) { exit 1 }
exit 0
