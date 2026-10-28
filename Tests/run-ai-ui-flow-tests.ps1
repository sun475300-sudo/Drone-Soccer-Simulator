[CmdletBinding()]
param(
    [string]$EngineRoot='E:\Program Files\UE_5.8',
    [switch]$Packaged,
    [string]$PackageRoot='',
    [ValidateRange(960,3840)][int]$Width=1280,
    [ValidateRange(640,2160)][int]$Height=720
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $root 'Scripts\Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root -EngineRoot $EngineRoot -PackageRoot $PackageRoot
$output=Join-Path $root ('Saved\Verification\ai-ui-flow-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($output) | Out-Null
$userdata=Join-Path $output 'userdata'
[IO.Directory]::CreateDirectory($userdata) | Out-Null
$resultFile=Join-Path $output 'result.json'
$identity=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
# This is an explicit offscreen GPU fixture, not a headless or human/USB flight.
$arguments=@('-game','-RenderOffscreen','-ForceRes',('-ResX='+$Width),('-ResY='+$Height),'-nosound','-unattended','-nosplash','-ImperialNoWelcome',
    '-ImperialAIResultsSmoke','-ImperialAIResultsUIFlow','-ImperialAIResultsCase=fresh','-ImperialBridgeUrl=http://127.0.0.1:1',
    ('-ImperialAIResultsResult='+$resultFile),('-ImperialAIResultsCapture='+(Join-Path $output 'results-ko.png')),
    ('-ImperialDataDir='+$userdata),('-UserDir='+(Join-Path $output 'engine-user')),('-abslog='+(Join-Path $output 'runtime.log')))
if($Packaged){$exe=Join-Path $environment.PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator-Win64-Shipping.exe'}
else{$exe=Join-Path $environment.EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe';$arguments=@((Join-Path $root 'ImperialWingsSimulator.uproject'))+$arguments}
if(-not(Test-Path -LiteralPath $exe)){throw ('AI UI runtime missing: '+$exe)}
$quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
[IO.File]::WriteAllText((Join-Path $output 'invocation.json'),([ordered]@{executable=$exe;arguments=$arguments;source_sha256=$identity.source_sha256;scope='Explicit isolated native UI/goal diagnostic fixture; controlled initial placement, not physical pilot evidence.'} | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
Write-Output $output
$process=Start-Process -FilePath $exe -ArgumentList $quoted -WindowStyle Hidden -PassThru
if(-not $process.WaitForExit(150000)){Stop-Process -Id $process.Id;throw ('AI UI fixture timed out: '+$output)}
if(-not(Test-Path -LiteralPath $resultFile)){throw ('AI UI fixture result missing: '+$output)}
$result=Get-Content -LiteralPath $resultFile -Raw -Encoding UTF8 | ConvertFrom-Json
if($process.ExitCode -ne 0 -or -not $result.ok -or @($result.checks | Where-Object {-not $_.ok}).Count){
    $result.checks | Where-Object {-not $_.ok} | Format-Table
    throw ('AI UI flow failed: '+$resultFile)
}
if($result.build.source_sha256 -ne $identity.source_sha256){throw 'AI UI fixture/source identity mismatch'}
foreach($image in @('setup-ko.png','setup-en.png','results-ko.png','results-en.png')){
    if(-not(Test-Path -LiteralPath (Join-Path $output $image))){throw ('AI UI screenshot missing: '+$image)}
}
$summary=[ordered]@{ok=$true;build=$result.build;checks=$result.checks.Count;result=$resultFile;images=@('setup-ko.png','setup-en.png','results-ko.png','results-en.png');width=$Width;height=$Height;scope='Native Korean/English setup-result flow and active-setting-lock assertions. Rendered diagnostic screenshots; no OS input or controller validation.'}
[IO.File]::WriteAllText((Join-Path $output 'summary.json'),($summary | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
Write-Output ('PASS '+$result.checks.Count+' AI UI flow checks')
Write-Output (Join-Path $output 'summary.json')
