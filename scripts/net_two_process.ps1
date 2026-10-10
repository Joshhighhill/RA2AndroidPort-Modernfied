# scripts/net_two_process.ps1 -- two-process LAN test for engine/net.
# Starts two ra2netpeer processes on localhost (one per house), waits for both
# and checks that their hash trails are identical.
#   pwsh scripts/net_two_process.ps1 -Exe C:\ra2-build\net-t\ra2netpeer.exe [-Frames 900] [-ProtocolZero]
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [int]$Frames = 900,
    [int]$PortA = 41234,
    [int]$PortB = 41235,
    [switch]$ProtocolZero
)
$tmp = [System.IO.Path]::GetTempPath()
$outA = Join-Path $tmp "ra2netpeer_a.txt"
$outB = Join-Path $tmp "ra2netpeer_b.txt"
$extra = @()
if ($ProtocolZero) { $extra += "--protocol-zero" }
$a = Start-Process -FilePath $Exe -PassThru -NoNewWindow -RedirectStandardOutput $outA `
    -ArgumentList (@("--house", "0", "--port", "$PortA", "--peer", "127.0.0.1:$PortB", "--frames", "$Frames") + $extra)
$b = Start-Process -FilePath $Exe -PassThru -NoNewWindow -RedirectStandardOutput $outB `
    -ArgumentList (@("--house", "1", "--port", "$PortB", "--peer", "127.0.0.1:$PortA", "--frames", "$Frames") + $extra)
$a.WaitForExit(200000) | Out-Null
$b.WaitForExit(200000) | Out-Null
$ta = Get-Content $outA
$tb = Get-Content $outB
"--- house 0"; $ta
"--- house 1"; $tb
$trailA = ($ta | Where-Object { $_ -like "frame *" }) -join "`n"
$trailB = ($tb | Where-Object { $_ -like "frame *" }) -join "`n"
if ($trailA -ne "" -and $trailA -eq $trailB -and $a.ExitCode -eq 0 -and $b.ExitCode -eq 0) {
    "TWO-PROCESS PASS"; exit 0
}
"TWO-PROCESS FAIL"; exit 1
