[CmdletBinding()]
param(
    [string]$EngineRoot='',
    [switch]$Packaged,
    [string]$PackageRoot='',
    [switch]$RenderCapture,
    [ValidateRange(640,3840)][int]$Width=1600,
    [ValidateRange(480,2160)][int]$Height=1000,
    [switch]$SkipPersistenceRelaunch
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $root 'Scripts\Environment.ps1')
$environment=Get-ImperialEnvironment -ProjectRoot $root -EngineRoot $EngineRoot -PackageRoot $PackageRoot
$output=Join-Path $root ('Saved\Verification\radio-ux-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($output) | Out-Null
$userdata=Join-Path $output 'userdata'
$identity=& (Join-Path $root 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $root
$results=[Collections.Generic.List[object]]::new()
function Invoke-UXRun {
    param([string]$Name,[string[]]$Extra=@(),[switch]$Capture)
    $resultFile=Join-Path $output ($Name+'.json')
    $arguments=@('-game','-nullrhi','-nosound','-unattended','-nosplash','-ImperialNoWelcome','-ImperialRadioUXSmoke','-ImperialBridgeUrl=http://127.0.0.1:1',('-ImperialRadioUXResult='+$resultFile),('-ImperialDataDir='+$userdata),('-abslog='+(Join-Path $output ($Name+'.log'))))+$Extra
    if($Capture){$arguments=@($arguments | Where-Object {$_ -ne '-nullrhi'})+@('-RenderOffscreen','-ForceRes',('-ResX='+$Width),('-ResY='+$Height),('-ImperialRadioUXCapture='+$output))}
    if($Packaged){$exe=Join-Path $environment.PackageRoot 'ImperialWingsSimulator\Binaries\Win64\ImperialWingsSimulator-Win64-Shipping.exe'}
    else{$exe=Join-Path $environment.EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe';$arguments=@((Join-Path $root 'ImperialWingsSimulator.uproject'))+$arguments}
    if(-not(Test-Path -LiteralPath $exe)){throw ('Runtime executable not found: '+$exe)}
    $quoted=@($arguments | ForEach-Object {'"'+$_+'"'})
    $process=Start-Process -FilePath $exe -ArgumentList $quoted -WindowStyle Hidden -PassThru
    $deadline=[DateTime]::UtcNow.AddSeconds(90)
    while(-not $process.WaitForExit(1000)){
        if([DateTime]::UtcNow -gt $deadline){Stop-Process -Id $process.Id;throw ('Controller UX fixture timed out: '+$Name+'; '+$output)}
    }
    if(-not(Test-Path -LiteralPath $resultFile)){throw ('No controller UX result: '+$output)}
    $result=Get-Content -LiteralPath $resultFile -Raw -Encoding UTF8 | ConvertFrom-Json
    if($result.build.source_sha256 -ne $identity.source_sha256){throw ('Controller UX fixture/source identity mismatch: '+$Name)}
    if($process.ExitCode -ne 0 -or -not $result.ok){$result.checks | Where-Object {-not $_.ok} | Format-Table;throw ('Controller UX verification failed: '+$resultFile)}
    $results.Add([pscustomobject]@{name=$Name;checks=@($result.checks).Count;result=$resultFile;ok=$result.ok})
    Write-Output ('PASS '+@($result.checks).Count+' controller UX checks: '+$Name)
}
Invoke-UXRun -Name 'initial' -Capture:$RenderCapture
if(-not $SkipPersistenceRelaunch){
    Invoke-UXRun -Name 'persisted' -Extra @('-ImperialRadioUXRestoreOnly','-ImperialRadioUXExpectedLanguage=en')
    Invoke-UXRun -Name 'override' -Extra @('-ImperialRadioUXRestoreOnly','-ImperialRadioUXExpectedLanguage=ko','-ImperialLanguage=ko')
}
if($RenderCapture){foreach($name in @('controller-ko.png','controller-en.png','response-basic-ko.png','response-advanced-ko.png')){if(-not(Test-Path -LiteralPath (Join-Path $output $name))){throw ('Missing capture: '+$name)}}}
$summary=[pscustomobject]@{ok=$true;build=$identity;runs=$results.ToArray();scope='isolated UI/language persistence and atomic-file failure checks; no physical controller or different physical PC test'}
[IO.File]::WriteAllText((Join-Path $output 'summary.json'),($summary | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
Write-Output (Join-Path $output 'summary.json')
