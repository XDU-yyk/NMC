param(
    [string]$Compiler = "g++"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = Join-Path $RepoRoot ".pio\build"
$IncludeDir = Join-Path $RepoRoot "src"

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$tests = @(
    @{
        Name = "manual_control"
        Sources = @(
            "test\test_manual_control.cpp",
            "src\control\manual_control.cpp"
        )
        Output = "manual_control_test.exe"
    },
    @{
        Name = "fc_ready_logic"
        Sources = @(
            "test\test_fc_ready_logic.cpp"
        )
        Output = "fc_ready_logic_test.exe"
    },
    @{
        Name = "fc_ready_web_output_path"
        Sources = @(
            "test\test_fc_ready_web_output_path.cpp",
            "src\control\manual_control.cpp"
        )
        Output = "fc_ready_web_output_path_test.exe"
    }
)

foreach ($test in $tests) {
    Write-Host ""
    Write-Host "== Building $($test.Name) =="

    $sources = @()
    foreach ($source in $test.Sources) {
        $sources += (Join-Path $RepoRoot $source)
    }

    $exe = Join-Path $BuildDir $test.Output
    & $Compiler -std=c++17 -I $IncludeDir @sources -o $exe
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed: $($test.Name)"
    }

    Write-Host "== Running $($test.Name) =="
    & $exe
    if ($LASTEXITCODE -ne 0) {
        throw "Test failed: $($test.Name)"
    }
}

Write-Host ""
Write-Host "All host tests passed."
