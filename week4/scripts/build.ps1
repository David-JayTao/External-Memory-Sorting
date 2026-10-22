param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [switch]$Demo
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $projectRoot "build/$Configuration"
$pythonScripts = Join-Path $env:USERPROFILE 'miniforge3/Scripts'
$compilerBin = Join-Path $projectRoot '.tools/w64devkit/bin'
if (-not (Test-Path (Join-Path $compilerBin 'g++.exe'))) {
    $compilerBin = Join-Path (Split-Path $projectRoot -Parent) '.tools/w64devkit/bin'
}
# Change PATH only for this script process, not the machine/user environment.
$originalPath = $env:PATH
try {
    $env:PATH = "$compilerBin;$pythonScripts;$env:PATH"
    $cmake = (Get-Command cmake.exe -ErrorAction Stop).Source
    & $cmake -S $projectRoot -B $buildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & $cmake --build $buildDir
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    & $cmake -E chdir $buildDir ctest --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    if ($Demo) {
        $outputFile = Join-Path $buildDir ("demo-" + [guid]::NewGuid().ToString('N'))
        & (Join-Path $buildDir 'week4_demo.exe') $outputFile
        if ($LASTEXITCODE -ne 0) { throw 'Demo failed' }
        Write-Host "Demo file: $outputFile"
    }
} finally { $env:PATH = $originalPath }
