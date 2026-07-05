param(
    [string]$Compiler = "g++",
    [string]$PlatformIO = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

if ([string]::IsNullOrWhiteSpace($PlatformIO)) {
    $DefaultPio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"
    if (Test-Path -LiteralPath $DefaultPio) {
        $PlatformIO = $DefaultPio
    } else {
        $PlatformIO = "platformio"
    }
}

Write-Host "== Replacement-FC software checks =="
Write-Host "Repo: $RepoRoot"
Write-Host "PlatformIO: $PlatformIO"
Write-Host ""
Write-Host "Safety: this script builds and runs software checks only."
Write-Host "Safety: it does not upload firmware, open serial ports, arm, write MSP, or run motors."

Write-Host ""
Write-Host "== Static safety checks =="
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_replacement_fc_static_checks.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "Static safety checks failed"
}

Write-Host ""
Write-Host "== Host safety tests =="
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_host_tests.ps1") -Compiler $Compiler
if ($LASTEXITCODE -ne 0) {
    throw "Host tests failed"
}

$envs = @(
    "esp32-s3-unified-web",
    "esp32-s3-unified-web-fc-ready",
    "esp32-s3-fc-uart-probe",
    "esp32-s3-fc-diag"
)

foreach ($envName in $envs) {
    Write-Host ""
    Write-Host "== PlatformIO build: $envName =="
    & $PlatformIO run -e $envName --disable-auto-clean
    if ($LASTEXITCODE -ne 0) {
        throw "PlatformIO build failed: $envName"
    }
}

Write-Host ""
Write-Host "Replacement-FC software checks passed."
Write-Host "Next hardware evidence still required: Receiver, no-prop Motors, failsafe, and low-altitude MC6C direction acceptance."
