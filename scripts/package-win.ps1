# W6-06-02: build the Windows release folder + zip.
#
#   pwsh scripts/package-win.ps1 [-Build build-rel] [-Out dist] [-Configure]
#
# -Configure runs a fresh `cmake -G Ninja -DCMAKE_BUILD_TYPE=Release` into
# -Build first; otherwise the existing build dir is rebuilt (ra2engine,
# ra2game). The result is <Out>/ra2engine-<version>-win64/ and the same
# folder zipped next to it:
#
#   ra2game.exe          the player's binary (no console window)
#   ra2engine.exe        the console binary (logs in the terminal, --help)
#   SDL2.dll
#   INSTALL.md, LICENSE, THIRD_PARTY_NOTICES.md
#
# The MinGW C++ runtime is linked statically (CMakeLists.txt), so nothing
# else is needed. The script checks this: every DLL the two exes and
# SDL2.dll import must be a Windows system DLL or SDL2.dll, and
# `ra2engine.exe --version` must run from the package folder with PATH
# limited to C:\Windows\system32 (a clean machine).
[CmdletBinding()]
param(
    [string]$Build = "build-rel",
    [string]$Out = "dist",
    [switch]$Configure
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    if ($Configure -or -not (Test-Path (Join-Path $Build "build.ninja"))) {
        cmake -S . -B $Build -G Ninja -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    }
    cmake --build $Build --target ra2engine ra2game
    if ($LASTEXITCODE -ne 0) { throw "build failed" }

    $exe = Join-Path $Build "ra2engine.exe"
    $ver = (& $exe --version 2>&1 | Select-Object -First 1).ToString().Trim()
    # "Red Alert 2 engine 0.9.0+g1a2b3c4" -> "0.9.0+g1a2b3c4"
    $ver = ($ver -split ' ')[-1]
    $safe = $ver -replace '[^0-9A-Za-z.+-]', '_' -replace '\+', '-'
    $name = "ra2engine-$safe-win64"
    $dst = Join-Path $Out $name
    if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
    New-Item -ItemType Directory -Force $dst | Out-Null

    Copy-Item (Join-Path $Build "ra2engine.exe") $dst
    Copy-Item (Join-Path $Build "ra2game.exe") $dst
    Copy-Item (Join-Path $Build "SDL2.dll") $dst
    Copy-Item "LICENSE", "THIRD_PARTY_NOTICES.md" $dst
    Copy-Item "docs/INSTALL.md" $dst

    # Import check: only system DLLs + SDL2.dll.
    $objdump = (Get-Command objdump -ErrorAction SilentlyContinue).Source
    if ($objdump) {
        $system = @("kernel32.dll", "user32.dll", "gdi32.dll", "winmm.dll", "imm32.dll", "ole32.dll",
                    "oleaut32.dll", "version.dll", "advapi32.dll", "setupapi.dll", "shell32.dll",
                    "msvcrt.dll", "ws2_32.dll", "dinput8.dll", "dxgi.dll", "d3d9.dll", "d3d11.dll",
                    "cfgmgr32.dll", "hid.dll", "sdl2.dll", "comdlg32.dll", "shlwapi.dll", "uuid.dll")
        foreach ($f in "ra2engine.exe", "ra2game.exe", "SDL2.dll") {
            $dlls = & $objdump -p (Join-Path $dst $f) | Select-String "DLL Name: (.*)" |
                    ForEach-Object { $_.Matches[0].Groups[1].Value.ToLower() }
            $bad = $dlls | Where-Object { $_ -notin $system -and $_ -notlike "api-ms-win-*" }
            if ($bad) { throw "$f needs non-system DLLs: $($bad -join ', ')" }
            Write-Host "imports ok: $f ($($dlls -join ' '))"
        }
    } else {
        Write-Warning "objdump not found: skipping the import check"
    }

    # Clean-folder run: PATH limited to Windows itself.
    $savedPath = $env:PATH
    try {
        $env:PATH = "$env:SystemRoot\system32;$env:SystemRoot"
        $o = & (Join-Path $dst "ra2engine.exe") --version 2>&1
        $rc = $LASTEXITCODE
    } finally { $env:PATH = $savedPath }
    if ($rc -ne 0) { throw "ra2engine.exe --version from the package exited $rc" }
    Write-Host "clean-folder run ok: $o"

    $zip = Join-Path $Out "$name.zip"
    if (Test-Path $zip) { Remove-Item -Force $zip }
    Compress-Archive -Path $dst -DestinationPath $zip
    Write-Host "package: $((Resolve-Path $dst).Path)"
    Write-Host "zip:     $((Resolve-Path $zip).Path)"
} finally {
    Pop-Location
}
