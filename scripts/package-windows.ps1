# Builds dist\PDF-Handler-<version>-Setup.exe from a configured+built CMake build dir.
#   pwsh scripts/package-windows.ps1 [build-dir]
param([string]$Build = "build/ci-windows")
$ErrorActionPreference = "Stop"

$Root = Resolve-Path "$PSScriptRoot/.."
$Build = Resolve-Path $Build
$Version = (Select-String -Path "$Root/CMakeLists.txt" -Pattern '^\s*VERSION ([0-9.]+)$').Matches[0].Groups[1].Value
$Stage = Join-Path $Build "stage"
$Dist = Join-Path $Root "dist"

if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path $Dist | Out-Null
cmake --install $Build --prefix $Stage   # runs windeployqt via Qt's deploy script

$Iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $Iscc)) {
    choco install innosetup --no-progress -y | Out-Null
}
& $Iscc "/DAppVersion=$Version" "/DSourceDir=$Stage" "/DIconFile=$Build\app\icons\app.ico" `
        "/DOutputDir=$Dist" "$Root\packaging\windows\installer.iss"
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed" }
Write-Host "Built $Dist\PDF-Handler-$Version-Setup.exe"
