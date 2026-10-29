[CmdletBinding()]
param([switch]$Packaged,[string]$PackageRoot='',[switch]$ExpectFailure)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $root 'Scripts\Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root -PackageRoot $PackageRoot
$output=Join-Path $root ('Saved\Verification\input-safety-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $output | Out-Null
$resultFile=Join-Path $output 'result.json'
$arguments=@('-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome','-ImperialInputSmoke','-ImperialBridgeUrl=http://127.0.0.1:1',('-ImperialInputResult='+$resultFile),('-ImperialDataDir='+(Join-Path $output 'userdata')),('-UserDir='+(Join-Path $output 'engine-user')),('-abslog='+(Join-Path $output 'runtime.log')))
if($Packaged){$exe=Join-Path $environment.PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator-Win64-Shipping.exe'}
else{$exe=Join-Path $environment.EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe';$arguments=@((Join-Path $root 'ImperialWingsSimulator.uproject'))+$arguments}
$quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
$p=Start-Process -FilePath $exe -ArgumentList $quoted -WindowStyle Hidden -PassThru
if(-not $p.WaitForExit(60000)){Stop-Process -Id $p.Id;throw 'Input fixture timed out'}
if(-not(Test-Path -LiteralPath $resultFile)){throw 'Input fixture produced no result'}
$result=Get-Content -LiteralPath $resultFile -Raw | ConvertFrom-Json
$identity=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
if($result.build.source_sha256 -ne $identity.source_sha256){throw 'Input fixture/source identity mismatch'}
$failed=@($result.checks | Where-Object {-not $_.ok})
if($ExpectFailure){if($result.ok -or $failed.Count -eq 0){throw 'Expected baseline failure was not reproduced'};Write-Output ('Expected baseline failures: '+($failed.check -join '; '))}
elseif($p.ExitCode -ne 0 -or -not $result.ok){$failed | Format-Table;throw ('Input verification failed: '+$resultFile)}
else{Write-Output ('PASS '+@($result.checks).Count+' engine input safety checks')}
Write-Output $resultFile
