[CmdletBinding()]
param([switch]$Packaged,[string]$PackageRoot='',[switch]$RenderCapture)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $root 'Scripts\Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root -PackageRoot $PackageRoot
$output=Join-Path $root ('Saved\Verification\radio-modes-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
$resultFile=Join-Path $output 'result.json'
$arguments=@('-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome','-ImperialRadioModeSmoke','-ImperialBridgeUrl=http://127.0.0.1:1',('-ImperialRadioModeResult='+$resultFile),('-ImperialDataDir='+(Join-Path $output 'userdata')),('-UserDir='+(Join-Path $output 'engine-user')),('-abslog='+(Join-Path $output 'runtime.log')))
if($RenderCapture){$arguments=@($arguments | Where-Object {$_ -ne '-nullrhi'})+@('-RenderOffscreen','-ResX=1600','-ResY=1000',('-ImperialRadioModeCapture='+$output))}
if($Packaged){$exe=Join-Path $environment.PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator-Win64-Shipping.exe'}
else{$exe=Join-Path $environment.EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe';$arguments=@((Join-Path $root 'ImperialWingsSimulator.uproject'))+$arguments}
$quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
$p=Start-Process -FilePath $exe -ArgumentList $quoted -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(60000)){Stop-Process -Id $p.Id;throw 'Controller mode fixture timed out'}
if(-not(Test-Path -LiteralPath $resultFile)){throw ('No controller mode result: '+$output)}
$result=Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
$identity=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
if($result.build.source_sha256 -ne $identity.source_sha256){throw 'Mode fixture/source identity mismatch'}
if($RenderCapture){foreach($name in @('controller-mode1.png','controller-mode2.png')){if(-not(Test-Path -LiteralPath (Join-Path $output $name))){throw ('Missing capture: '+$name)}}}
if($p.ExitCode -ne 0 -or -not $result.ok){$result.checks | Where-Object {-not $_.ok} | Format-Table;throw ('Controller mode verification failed: '+$resultFile)}
Write-Output ('PASS '+@($result.checks).Count+' controller mode UI/JSON checks')
Write-Output $resultFile
