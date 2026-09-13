$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} `
    "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found."
}

$installation = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $installation) {
    throw "Visual Studio C++ tools were not found."
}

$developerCommand = Join-Path $installation "Common7\Tools\VsDevCmd.bat"
$outputDirectory = Join-Path $projectRoot "build\host-tests"
$output = Join-Path $outputDirectory "texture_performance_tests.exe"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

$compileCommand =
    "call `"$developerCommand`" -arch=amd64 -host_arch=amd64 >nul && " +
    "cl.exe /nologo /std:c++20 /utf-8 /EHsc /W4 /WX " +
    "/I`"$projectRoot\tests\stubs`" /I`"$projectRoot\include`" " +
    "`"$projectRoot\source\textures\TextureFingerprint.cpp`" " +
    "`"$projectRoot\tests\texture_performance_tests.cpp`" " +
    "/Fo$outputDirectory\ /Fe$output bcrypt.lib"

& $env:ComSpec /d /c $compileCommand
if ($LASTEXITCODE -ne 0) {
    throw "Host test compilation failed with exit code $LASTEXITCODE."
}

& $output
if ($LASTEXITCODE -ne 0) {
    throw "Host tests failed with exit code $LASTEXITCODE."
}
