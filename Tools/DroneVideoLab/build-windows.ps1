$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Push-Location $PSScriptRoot
try {
  if (-not (Test-Path '.venv/Scripts/python.exe')) {
    python -m venv .venv
    if ($LASTEXITCODE -ne 0) { throw 'venv creation failed' }
  }
  $python = (Resolve-Path '.venv/Scripts/python.exe').Path
  function Invoke-Python { & $python @args; if ($LASTEXITCODE -ne 0) { throw "Python step failed: $args" } }
  New-Item -ItemType Directory -Force verification,release | Out-Null
  Invoke-Python -m pip install -r requirements-build.txt
  Invoke-Python -m unittest discover -s tests -v
  Invoke-Python app.py --smoke-test verification/source
  Invoke-Python tools/collect_notices.py
  Invoke-Python -m PyInstaller --noconfirm --clean --onefile --windowed --name DroneVideoLab --collect-all cv2 --copy-metadata numpy --copy-metadata opencv-python --copy-metadata Pillow --add-data 'licenses;licenses' app.py
  $relocated = Join-Path $env:TEMP ('드론 영상 검증 ' + $PID)
  New-Item -ItemType Directory -Force $relocated | Out-Null
  Copy-Item dist/DroneVideoLab.exe (Join-Path $relocated 'DroneVideoLab.exe')
  $smoke = Join-Path $PSScriptRoot 'verification/relocated-exe'
  $process = Start-Process -FilePath (Join-Path $relocated 'DroneVideoLab.exe') -ArgumentList @('--smoke-test',('"'+$smoke+'"')) -Wait -PassThru
  if ($process.ExitCode -ne 0) { throw "Relocated EXE smoke test failed: $($process.ExitCode)" }
  $result = Get-Content (Join-Path $smoke 'result.json') -Raw | ConvertFrom-Json
  if (-not $result.passed) { throw 'EXE smoke result did not pass' }
  $exeName='DroneVideoLab-0.1.0-preview-win-x64.exe'
  Copy-Item dist/DroneVideoLab.exe (Join-Path release $exeName)
  New-Item -ItemType Directory -Force release/portable | Out-Null
  Copy-Item dist/DroneVideoLab.exe release/portable/DroneVideoLab.exe
  Copy-Item README.md release/portable/README.md
  Copy-Item licenses release/portable/licenses -Recurse -Force
  Compress-Archive -Path release/portable/* -DestinationPath release/DroneVideoLab-0.1.0-preview-win-x64.zip -Force
  Compress-Archive -Path app.py,core.py,requirements.txt,requirements-build.txt,build-windows.ps1,README.md,tests,tools -DestinationPath release/DroneVideoLab-source.zip -Force
  Compress-Archive -Path verification/* -DestinationPath release/DroneVideoLab-verification.zip -Force
  $lines=Get-ChildItem release -File | Sort-Object Name | ForEach-Object { (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower() + '  ' + $_.Name }
  $lines | Set-Content release/SHA256SUMS.txt -Encoding utf8
  Write-Host 'Windows EXE built, launched from a relocated Unicode path, and smoke-tested.'
} finally { Pop-Location }
