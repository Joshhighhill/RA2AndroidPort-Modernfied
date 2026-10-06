# Run --play scripts ONE AT A TIME (each loads the full asset set) and print a
# summary. Usage (from the repo root):
#   pwsh scripts/play/run-all.ps1 [-Exe build/ra2engine.exe] [-Out build/play-out] [script ...]
# With no scripts: the core set (sk-a sk-s sk-end sk-a-lose m-all01 m-sov01 shell-smoke).
param(
    [string]$Exe = "build/ra2engine.exe",
    [string]$Out = "build/play-out",
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$Scripts
)
if (-not $Scripts -or $Scripts.Count -eq 0) {
    $Scripts = @("sk-a", "sk-s", "sk-end", "sk-a-lose", "m-all01", "m-sov01", "shell-smoke")
}
$rows = @()
foreach ($s in $Scripts) {
    $path = if (Test-Path $s) { $s } else { "scripts/play/$s.play" }
    $stem = [IO.Path]::GetFileNameWithoutExtension($path)
    $dir = Join-Path $Out $stem
    Write-Host "=== $stem"
    & $Exe --play $path --out $dir 2>&1 | Select-String -Pattern "^PLAY (FAIL|scene)|^play_(result|hash)" | ForEach-Object { Write-Host $_.Line }
    $res = Join-Path $dir "result.txt"
    $first = if (Test-Path $res) { (Get-Content $res -TotalCount 2) -join " " } else { "no result.txt (crash?) exit=$LASTEXITCODE" }
    $rows += [pscustomobject]@{ Script = $stem; Exit = $LASTEXITCODE; Result = $first }
}
$rows | Format-Table -AutoSize -Wrap
