# Build simhook.dll + simlaunch.exe as 32-bit x86 with the MSVC that ships in
# the local Visual Studio 2022 (Hostx64\x86 cl). No installs needed.
#   pwsh tools/simdiff/hook/build.ps1 [-Out <dir>]
param([string]$Out = "$PSScriptRoot\..\..\..\build-simhook")
$ErrorActionPreference = 'Stop'
$vs = @(
  'C:\Program Files\Microsoft Visual Studio\2022\Community',
  'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
) | Where-Object { Test-Path "$_\VC\Auxiliary\Build\vcvarsall.bat" } | Select-Object -First 1
if (-not $vs) { throw 'no VS 2022 with vcvarsall.bat found' }
New-Item -ItemType Directory -Force $Out | Out-Null
$src = Resolve-Path $PSScriptRoot
$o = (Resolve-Path $Out).Path
$cmd = "`"$vs\VC\Auxiliary\Build\vcvarsall.bat`" x64_x86 >nul && cd /d `"$o`" && " +
       "cl /nologo /O1 /W3 /D_CRT_SECURE_NO_WARNINGS /MT /LD `"$src\simhook.c`" /Fe:simhook.dll user32.lib kernel32.lib && " +
       "cl /nologo /O1 /W3 /D_CRT_SECURE_NO_WARNINGS /MT `"$src\simlaunch.c`" /Fe:simlaunch.exe kernel32.lib"
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
Get-ChildItem $o -Filter 'sim*.???' | Select-Object Name, Length
