# NET-PLAY two-process network MATCH test: a hosting game and a joining game
# on this PC play a real lockstep match (scripts/play/net-*-host.play +
# net-*-join.play): lobby, start, build, train, attack, 3000 logic frames.
# Both log their sync-hash trail ("[net] trail f=N crc=X" on stderr, one line
# per hash period); this script requires the two trails to be IDENTICAL over
# the whole match and the per-house outcomes to agree.
#   pwsh scripts/play/net-match.ps1 [-Exe build/ra2engine.exe] [-Out build/play-out]
#        [-Online] [-Server path\ra2lobbyd.exe]     # through ra2lobbyd + the V3 tunnel
#        [-DesyncAt <frame>]                        # inject a divergence on the JOINER
#        [-Fuzz]                                    # W9-05-12: the JOINER also sends forged packets
#        [-VersionMismatch]                         # W9-05-02: the JOINER is "another build"
# -Fuzz expects a normal PASS (identical trails) AND the host's "[net] rejected:"
# line to count dropped packets/events. -VersionMismatch expects NO match: the
# joiner is refused in the lobby with "Different game version" and the host
# logs "could not join: different game version".
# -DesyncAt expects the opposite: both machines stop with "out of sync", the
# trails agree before the frame and differ after, and each machine wrote its
# DESYNC_F<frame>_H<house>.SAV/.TXT pair to its saves folder.
# Needs RA2_INSTALL_DIR. Takes several minutes (two full games).
[CmdletBinding()]
param([string]$Exe = "build/ra2engine.exe", [string]$Out = "build/play-out", [switch]$Online,
      [string]$Server = "", [int]$DesyncAt = -1, [switch]$Fuzz, [switch]$VersionMismatch)
$ErrorActionPreference = "Stop"
$kind = if ($Online) { "online" } else { "lan" }
$tag = if ($DesyncAt -ge 0) { "net-$kind-desync" } elseif ($Fuzz) { "net-$kind-fuzz" } elseif ($VersionMismatch) { "net-$kind-version" } else { "net-$kind" }
New-Item -ItemType Directory -Force $Out | Out-Null
$hostOut = Join-Path $Out "$tag-host"; $joinOut = Join-Path $Out "$tag-join"
foreach ($d in @($hostOut, $joinOut)) { if (Test-Path $d) { Remove-Item -Recurse -Force $d }; New-Item -ItemType Directory -Force $d | Out-Null }
$srv = $null
if ($Online) {
    if (-not $Server) {
        $Server = Join-Path (Resolve-Path $Out) "ra2lobbyd.exe"
        Push-Location server; go build -o $Server ./cmd/ra2lobbyd; Pop-Location
    }
    $srv = Start-Process -FilePath $Server -ArgumentList @("--allow-loopback", "--server-name", "`"Loopback Test`"", "--relay-ports", "50100-50109", "--admin", "127.0.0.1:18080") `
           -RedirectStandardError (Join-Path $Out "$tag-ra2lobbyd.log") -RedirectStandardOutput (Join-Path $Out "$tag-ra2lobbyd.out") -PassThru -NoNewWindow
    Start-Sleep -Seconds 1
}
$hostErr = Join-Path $hostOut "stderr.txt"; $joinErr = Join-Path $joinOut "stderr.txt"
$h = Start-Process -FilePath $Exe -ArgumentList @("--play", "scripts/play/net-$kind-host.play", "--out", $hostOut) `
     -RedirectStandardOutput (Join-Path $hostOut "stdout.txt") -RedirectStandardError $hostErr -PassThru -NoNewWindow
$ready = if ($Online) { "room created code=" } else { "hosting LAN game" }
$deadline = (Get-Date).AddSeconds(120)
while ((Get-Date) -lt $deadline -and -not (Select-String -Path $hostErr -Pattern $ready -Quiet -ErrorAction SilentlyContinue)) { Start-Sleep -Milliseconds 500 }
if ($DesyncAt -ge 0) { $env:RA2_NET_DESYNC_AT = "$DesyncAt" }
if ($Fuzz) { $env:RA2_NET_FUZZ = "1" }
if ($VersionMismatch) { $env:RA2_NET_SIM_SALT = "another-build" }
$j = Start-Process -FilePath $Exe -ArgumentList @("--play", "scripts/play/net-$kind-join.play", "--out", $joinOut) `
     -RedirectStandardOutput (Join-Path $joinOut "stdout.txt") -RedirectStandardError $joinErr -PassThru -NoNewWindow
Remove-Item Env:RA2_NET_DESYNC_AT -ErrorAction SilentlyContinue
Remove-Item Env:RA2_NET_FUZZ -ErrorAction SilentlyContinue
Remove-Item Env:RA2_NET_SIM_SALT -ErrorAction SilentlyContinue
if ($VersionMismatch) {
    # W9-05-02: both sides must say so in the lobby; the match never starts.
    $deadline = (Get-Date).AddSeconds(240)
    $hm = $null; $jm = $null
    while ((Get-Date) -lt $deadline -and -not ($hm -and $jm)) {
        $hm = Select-String -Path $hostErr -Pattern "could not join: different game version.*" -ErrorAction SilentlyContinue | Select-Object -First 1
        $jm = Select-String -Path $joinErr -Pattern "join refused: Different game version.*" -ErrorAction SilentlyContinue | Select-Object -First 1
        Start-Sleep -Seconds 1
    }
    Start-Sleep -Seconds 2
    $started = (Select-String -Path $hostErr -Pattern "\[net\] lockstep match" -Quiet -ErrorAction SilentlyContinue) -or
               (Select-String -Path $joinErr -Pattern "\[net\] lockstep match" -Quiet -ErrorAction SilentlyContinue)
    foreach ($p in @($h, $j)) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }   # only the PIDs started here
    if ($srv) { Stop-Process -Id $srv.Id -Force }
    Write-Host ("host: " + $(if ($hm) { $hm.Matches[0].Value } else { "NO version message" }))
    Write-Host ("join: " + $(if ($jm) { $jm.Matches[0].Value } else { "NO version message" }))
    Write-Host ("match started: " + $started)
    $pass = $hm -and $jm -and -not $started
    Write-Host ("version mismatch gate: " + $(if ($pass) { "PASS" } else { "FAIL" }))
    if (-not $pass) { exit 1 }
    exit 0
}
if ($DesyncAt -ge 0) {
    # The match stops at the out-of-sync; the scripts would then wait out their
    # budgets -- end both runs once both machines reported the end (+ dumps).
    $deadline = (Get-Date).AddSeconds(1500)
    while ((Get-Date) -lt $deadline -and -not ($h.HasExited -and $j.HasExited)) {
        $he = Select-String -Path $hostErr -Pattern "\[net\] match ended" -Quiet -ErrorAction SilentlyContinue
        $je = Select-String -Path $joinErr -Pattern "\[net\] match ended" -Quiet -ErrorAction SilentlyContinue
        if ($he -and $je) { Start-Sleep -Seconds 3; break }
        Start-Sleep -Seconds 2
    }
}
$ok = if ($DesyncAt -ge 0) { $true } else { $j.WaitForExit(1500000) -and $h.WaitForExit(300000) }
foreach ($p in @($h, $j)) { if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force } }   # only the PIDs started here
$fwd = -1
if ($srv) {
    $metrics = try { (Invoke-WebRequest -UseBasicParsing http://127.0.0.1:18080/metrics -TimeoutSec 3).Content } catch { "" }
    Stop-Process -Id $srv.Id -Force
    $fwd = if ($metrics -match "ra2lobbyd_v3_forwarded_total (\d+)") { [int]$Matches[1] } else { -1 }
}

function Trail([string]$path) {
    $t = @{}
    foreach ($m in (Select-String -Path $path -Pattern "\[net\] trail f=(\d+) crc=([0-9a-f]+)" -ErrorAction SilentlyContinue)) {
        $t[[int]$m.Matches[0].Groups[1].Value] = $m.Matches[0].Groups[2].Value
    }
    return $t
}
function Outcomes([string]$path) {
    # The final `dump` writes the whole probe: net_house_out(h)=v
    $o = @{}
    foreach ($m in (Select-String -Path $path -Pattern "net_house_out\((\d+)\)\s*[=:]\s*(-?\d+)" -ErrorAction SilentlyContinue)) {
        $o[[int]$m.Matches[0].Groups[1].Value] = [int]$m.Matches[0].Groups[2].Value
    }
    return $o
}
$ht = Trail $hostErr; $jt = Trail $joinErr
$common = @($ht.Keys | Where-Object { $jt.ContainsKey($_) } | Sort-Object)
$diff = @($common | Where-Object { $ht[$_] -ne $jt[$_] })
$maxf = if ($common.Count) { $common[-1] } else { -1 }
$hr = Get-Content (Join-Path $hostOut "result.txt") -TotalCount 1 -ErrorAction SilentlyContinue
$jr = Get-Content (Join-Path $joinOut "result.txt") -TotalCount 1 -ErrorAction SilentlyContinue
Write-Host "host: $hr (exit $($h.ExitCode))"
Write-Host "join: $jr (exit $($j.ExitCode))"
Write-Host ("hash trail: {0} common entries up to frame {1}, {2} differ" -f $common.Count, $maxf, $diff.Count)
if ($Online) { Write-Host "V3 tunnel datagrams forwarded by ra2lobbyd: $fwd" }
$chat = Select-String -Path $hostErr -Pattern "\[net\] chat Joiner: gl hf" -Quiet -ErrorAction SilentlyContinue
Write-Host ("in-game chat joiner -> host: " + $(if ($chat) { "received" } else { "MISSING" }))
foreach ($s in @(@("host", $hostErr), @("join", $joinErr))) {
    $hl = Select-String -Path $s[1] -Pattern "\[net\] sync hash: .*" -ErrorAction SilentlyContinue | Select-Object -Last 1
    if ($hl) { Write-Host ("{0} {1}" -f $s[0], $hl.Matches[0].Value) }
}

if ($DesyncAt -ge 0) {
    $hoos = Select-String -Path $hostErr -Pattern "\[net\] match ended: out-of-sync.*" -ErrorAction SilentlyContinue | Select-Object -First 1
    $joos = Select-String -Path $joinErr -Pattern "\[net\] match ended: out-of-sync.*" -ErrorAction SilentlyContinue | Select-Object -First 1
    $hd = @(Get-ChildItem -Recurse -Path $hostOut -Filter "DESYNC_F*" -ErrorAction SilentlyContinue)
    $jd = @(Get-ChildItem -Recurse -Path $joinOut -Filter "DESYNC_F*" -ErrorAction SilentlyContinue)
    $before = @($common | Where-Object { $_ -le $DesyncAt -and $_ -gt 0 })
    $beforeOk = @($before | Where-Object { $ht[$_] -ne $jt[$_] }).Count -eq 0
    $after = @($common | Where-Object { $_ -gt $DesyncAt })
    $afterDiff = @($after | Where-Object { $ht[$_] -ne $jt[$_] }).Count -gt 0
    Write-Host ("host: " + $(if ($hoos) { $hoos.Matches[0].Value } else { "no out-of-sync" }))
    Write-Host ("join: " + $(if ($joos) { $joos.Matches[0].Value } else { "no out-of-sync" }))
    Write-Host ("desync dumps: host {0} file(s), join {1} file(s)" -f $hd.Count, $jd.Count)
    foreach ($f in ($hd + $jd)) { Write-Host ("  " + $f.FullName) }
    Write-Host ("trails: identical up to frame {0}: {1}; differ after: {2}" -f $DesyncAt, $beforeOk, $afterDiff)
    if (-not ($hoos -and $joos -and $hd.Count -ge 2 -and $jd.Count -ge 2 -and $beforeOk -and $afterDiff -and $before.Count -gt 5)) { exit 1 }
    exit 0
}
$ho = Outcomes (Join-Path $hostOut "play.log"); $jo = Outcomes (Join-Path $joinOut "play.log")
$outSame = $ho.Count -gt 0 -and $ho.Count -eq $jo.Count -and @($ho.Keys | Where-Object { $ho[$_] -ne $jo[$_] }).Count -eq 0
Write-Host ("outcomes host {0} / join {1}: {2}" -f (($ho.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join ","),
            (($jo.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join ","),
            $(if ($outSame) { "SAME" } else { "DIFFERENT / missing" }))
$away = (Select-String -Path $hostErr -Pattern "\[net\] away house 1" -Quiet -ErrorAction SilentlyContinue) -and
        (Select-String -Path $hostErr -Pattern "\[net\] back house 1" -Quiet -ErrorAction SilentlyContinue)
Write-Host ("Android away / back notices joiner -> host: " + $(if ($away) { "received" } else { "MISSING" }))
$pass = $h.ExitCode -eq 0 -and $j.ExitCode -eq 0 -and $diff.Count -eq 0 -and $maxf -ge 2985 -and $outSame -and $chat -and $away
if ($Fuzz) {
    # W9-05-12: the host dropped the joiner's forged packets / events.
    $rj = Select-String -Path $hostErr -Pattern "\[net\] rejected: (\d+) packets, (\d+) events.*" -ErrorAction SilentlyContinue | Select-Object -Last 1
    $rp = if ($rj) { [int]$rj.Matches[0].Groups[1].Value } else { 0 }
    $re = if ($rj) { [int]$rj.Matches[0].Groups[2].Value } else { 0 }
    Write-Host ("host " + $(if ($rj) { $rj.Matches[0].Value } else { "rejected: (no line)" }))
    if ($rp -lt 10 -or $re -lt 5) { $pass = $false }
}
if ($Online -and $fwd -le 0) { $pass = $false }
Write-Host ("network match: " + $(if ($pass) { "PASS" } else { "FAIL" }))
if (-not $pass) { exit 1 }
exit 0
