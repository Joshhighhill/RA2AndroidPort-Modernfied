# Cross-build ra2lobbyd for the usual VPS targets from Windows (needs Go 1.22+).
#   .\build.ps1          -> dist\ra2lobbyd-linux-amd64, dist\ra2lobbyd-linux-arm64, dist\ra2lobbyd.exe
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
New-Item -ItemType Directory -Force dist | Out-Null
$env:CGO_ENABLED = "0"
foreach ($t in @(@("linux","amd64","ra2lobbyd-linux-amd64"), @("linux","arm64","ra2lobbyd-linux-arm64"), @("windows","amd64","ra2lobbyd.exe"))) {
    $env:GOOS = $t[0]; $env:GOARCH = $t[1]
    go build -trimpath -ldflags="-s -w" -o ("dist\" + $t[2]) ./cmd/ra2lobbyd
    Write-Host ("built dist\" + $t[2])
}
Remove-Item Env:GOOS, Env:GOARCH
