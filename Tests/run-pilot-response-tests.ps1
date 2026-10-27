param([string]$VisualStudioRoot)
$ErrorActionPreference = 'Stop'
$projectDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $VisualStudioRoot) {
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswherePath) {
        $VisualStudioRoot = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
}
if (-not $VisualStudioRoot) { throw 'Visual Studio C++ build tools were not found. Pass -VisualStudioRoot.' }
$compilerVersion = Get-ChildItem -LiteralPath (Join-Path $VisualStudioRoot 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $compilerVersion) { throw 'No MSVC toolchain found.' }
$compilerPath = Join-Path $compilerVersion.FullName 'bin\Hostx64\x64\cl.exe'
$kitRoots = @(
    (Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots' -ErrorAction SilentlyContinue).KitsRoot10,
    (Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots' -ErrorAction SilentlyContinue).KitsRoot10,
    (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10')
)
$kitRoot = $kitRoots | Where-Object { $_ -and (Test-Path -LiteralPath (Join-Path $_ 'Include')) } | Select-Object -First 1
if (-not $kitRoot) { throw 'Windows SDK include directory was not found.' }
$kitVersion = Get-ChildItem -LiteralPath (Join-Path $kitRoot 'Include') -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'ucrt\stdio.h') } | Sort-Object Name -Descending | Select-Object -First 1
if (-not $kitVersion) { throw 'Windows SDK/UCRT headers were not found.' }
$outputDirectory = Join-Path $projectDirectory 'Saved\Verification\pilot-response-unit'
New-Item -Path $outputDirectory -ItemType Directory -Force | Out-Null
$testExecutable = Join-Path $outputDirectory 'pilot_response_tests.exe'
$moduleDirectory = Join-Path $projectDirectory 'Source\ImperialWingsSimulator'
$compilerArguments = @(
    '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/O2', '/MT',
    "/I$moduleDirectory", "/I$(Join-Path $compilerVersion.FullName 'include')", "/I$(Join-Path $kitVersion.FullName 'ucrt')",
    (Join-Path $PSScriptRoot 'pilot_response_tests.cpp'),
    "/Fe:$testExecutable", "/Fo:$outputDirectory\", '/link',
    "/LIBPATH:$(Join-Path $compilerVersion.FullName 'lib\x64')",
    "/LIBPATH:$(Join-Path $kitRoot "Lib\$($kitVersion.Name)\ucrt\x64")",
    "/LIBPATH:$(Join-Path $kitRoot "Lib\$($kitVersion.Name)\um\x64")"
)
& $compilerPath @compilerArguments
if ($LASTEXITCODE -ne 0) { throw 'Pilot response test compilation failed.' }
& $testExecutable
if ($LASTEXITCODE -ne 0) { throw 'Pilot response tests failed.' }
