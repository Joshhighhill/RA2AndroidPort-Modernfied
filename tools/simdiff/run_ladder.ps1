# simdiff ladder driver (docs/re/simdiff-harness.md). Runs the ORIGINAL game
# (a hard-linked copy, never the install itself) and the clone on the same
# scenarios, then scores every rung with simdiff.py. Heavy: run alone.
#
#   pwsh tools/simdiff/run_ladder.ps1 -Engine C:\ra2-h\build\ra2engine.exe `
#        -Out C:\ra2-h\runs [-Frames 3000] [-Seed 0x1234567] [-SkipOriginal]
param(
  [Parameter(Mandatory)] [string]$Engine,
  [Parameter(Mandatory)] [string]$Out,
  [string]$Install = 'D:\Red Alert 2',
  [string]$GameCopy = 'D:\ra2-re\h\game',
  [string]$HookDir = "$Out\simhook",
  [int]$Frames = 11000,   # the AI first hits the idle human near frame 10806
  [string]$Seed = '0x1234567',
  [switch]$SkipOriginal,
  [string[]]$Only = @()   # rerun only these original scenarios (e.g. orig_ai)
)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
New-Item -ItemType Directory -Force $Out | Out-Null

# 1. game copy: small files copied, big MIX/data files hard-linked (read-only use)
if (-not (Test-Path "$GameCopy\gamemd.exe")) {
  New-Item -ItemType Directory -Force $GameCopy | Out-Null
  Get-ChildItem $Install -File | ForEach-Object {
    $dst = Join-Path $GameCopy $_.Name
    if ($_.Length -gt 2MB -and $_.Extension -notin '.exe', '.dll', '.ini') {
      New-Item -ItemType HardLink -Path $dst -Target $_.FullName | Out-Null
    } else { Copy-Item $_.FullName $dst }
  }
  # small window; skirmish defaults the shell will start with
  $ini = Get-Content "$GameCopy\RA2MD.INI" -Raw
  $ini = $ini -replace 'ScreenWidth=\d+', 'ScreenWidth=800' -replace 'ScreenHeight=\d+', 'ScreenHeight=600'
  Set-Content "$GameCopy\RA2MD.INI" $ini -NoNewline
}

# 2. hook build
if (-not (Test-Path "$HookDir\simhook.dll")) { & "$here\hook\build.ps1" -Out $HookDir | Out-Null }

function Run-Original($name, $frames, $passive) {
  $env:SIMHOOK_OUT = "$Out\$name.txt"; $env:SIMHOOK_SEED = $Seed; $env:SIMHOOK_FRAMES = "$frames"
  $env:SIMHOOK_PASSIVE = "$passive"; $env:SIMHOOK_L0 = '100000'; $env:SIMHOOK_RNGTRACE = '0'
  # Startup is occasionally flaky (a crash or hang before the main menu, about
  # 1 in 10): retry until the hook logs "done at frame".
  for ($try = 1; $try -le 3; $try++) {
    Remove-Item "$Out\$name.txt*" -ErrorAction SilentlyContinue
    & "$HookDir\simlaunch.exe" $GameCopy gamemd.exe "$HookDir\simhook.dll" 1800 | Out-Host
    if (Select-String -Quiet -Path "$Out\$name.txt.log" -Pattern 'done at frame') { break }
    Write-Host "original run $name failed to start (try $try), retrying"
  }
  Get-Content "$Out\$name.txt.log" -Tail 1 | Out-Host
}
function Run-Clone($name, $frames, [string[]]$extra) {
  $env:RA2_INSTALL_DIR = $Install
  & $Engine --ai-soak --map xmp29u2.map --human-country YuriCountry --ai-country Russians `
     --no-human-deploy --long-game --starts 1,0 --frames $frames --diff-dump "$Out\$name.txt" @extra | Select-Object -Last 1 | Out-Host
}

# 3. scenarios
if (-not $SkipOriginal) {
  if (-not $Only -or 'orig_ai' -in $Only) { Run-Original 'orig_ai' $Frames 0 }
  if (-not $Only -or 'orig_ai_b' -in $Only) { Run-Original 'orig_ai_b' 600 0 }       # run-to-run determinism
  if (-not $Only -or 'orig_passive' -in $Only) { Run-Original 'orig_passive' 1500 2 } # house 1 idle from frame 0
}
if (-not $Only) {
  Run-Clone 'clone_ai' $Frames @()
  Run-Clone 'clone_passive' 1500 @('--ai-passive')
}
$l0 = Join-Path (Split-Path $Engine) 'simdiff_l0.exe'
if (Test-Path $l0) { & $l0 ($Seed -replace '^0x', '') 100000 "$Out\clone.l0" }

# 4. ladder
$m = @(
  @{ name = 'L0  RNG algorithm (1e5 raw + 1e5 ranged draws)'; kind = 'l0'; a = "$Out\orig_ai.txt.l0"; b = "$Out\clone.l0" },
  @{ name = 'ref original run-to-run (600 f)'; kind = 'diff'; a = "$Out\orig_ai.txt"; b = "$Out\orig_ai_b.txt"; frames = 600; ignore = @('rng2') },
  @{ name = 'L1  frame 0 (map load + start forces)'; kind = 'diff'; a = "$Out\orig_ai.txt"; b = "$Out\clone_ai.txt"; frames = 0; ignore = @('rng2') },
  @{ name = 'L2  map ticking (no actors, 1500 f)'; kind = 'diff'; a = "$Out\orig_passive.txt"; b = "$Out\clone_passive.txt"; ignore = @('rng2') },
  @{ name = 'L3  unit motion markers, original'; kind = 'markers'; a = "$Out\orig_ai.txt" },
  @{ name = 'L3  unit motion markers, clone'; kind = 'markers'; a = "$Out\clone_ai.txt" },
  @{ name = 'L4  combat (strict diff over the AI run)'; kind = 'diff'; a = "$Out\orig_ai.txt"; b = "$Out\clone_ai.txt"; ignore = @('rng2') },
  @{ name = 'L5  AI economy: build order of house 1'; kind = 'buildorder'; a = "$Out\orig_ai.txt"; b = "$Out\clone_ai.txt"; owner = 1 }
)
$m | ConvertTo-Json -Depth 4 | Set-Content "$Out\ladder.json"
python "$here\simdiff.py" ladder "$Out\ladder.json"
