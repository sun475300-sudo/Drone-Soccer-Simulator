param([Parameter(Mandatory=$true)][string]$Exe)
$ErrorActionPreference = 'Stop'
$exePath = (Resolve-Path $Exe).Path
$temp = Join-Path $env:TEMP ('DroneSoccerLab-test-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temp | Out-Null
$stdout = Join-Path $temp 'stdout.txt'
$stderr = Join-Path $temp 'stderr.txt'
$self = Start-Process -FilePath $exePath -ArgumentList '-self-test' -Wait -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
if ($self.ExitCode -ne 0 -or (Get-Content $stdout -Raw) -notmatch 'SELF_TEST_OK') { throw 'EXE self-test failed' }
$p = Start-Process -FilePath $exePath -ArgumentList '-no-open' -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
try {
  $url = $null
  for ($i=0; $i -lt 100; $i++) {
    Start-Sleep -Milliseconds 100
    if ($p.HasExited) { throw 'EXE exited before listening' }
    $text = Get-Content $stdout -Raw -ErrorAction SilentlyContinue
    if ($text -match '(http://127\.0\.0\.1:\d+/[a-f0-9]{48}/)') { $url = $Matches[1]; break }
  }
  if (-not $url) { throw 'No loopback URL emitted' }
  $health = Invoke-RestMethod ($url + 'health')
  if ($health.app -ne 'Drone Soccer Lab') { throw 'Wrong health response' }
  $index = Invoke-WebRequest $url
  if ($index.StatusCode -ne 200 -or $index.Content -notmatch 'app.js') { throw 'UI not served' }
  foreach ($asset in @('app.js','core.js','style.css')) {
    if ((Invoke-WebRequest ($url + $asset)).StatusCode -ne 200) { throw "Asset failed: $asset" }
  }
  $quit = Invoke-RestMethod ($url + 'quit') -Method Post -Headers @{'X-Lab-Request'='shutdown'}
  if (-not $quit.stopped) { throw 'Quit not acknowledged' }
  if (-not $p.WaitForExit(10000)) { throw 'EXE failed to exit' }
  if ($p.ExitCode -ne 0) { throw 'Non-zero exit' }
  Write-Output 'WINDOWS_EXE_SMOKE_OK: native self-test, HTTP, assets, shutdown'
} finally {
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -ErrorAction SilentlyContinue }
}
