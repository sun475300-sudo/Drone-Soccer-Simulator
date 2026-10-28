param([string]$ProjectRoot=(Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference='Stop'
$sourceRoot=[IO.Path]::GetFullPath($ProjectRoot)
if(-not(Test-Path -LiteralPath (Join-Path $sourceRoot 'ImperialWingsSimulator.uproject'))){throw '프로젝트 폴더에서 실행해 주세요.'}
$extensions=@('.h','.cpp','.cs','.ini','.json','.py','.ps1','.txt','.cmd','.bat','.uproject','.uasset','.umap','.png')
$binary=@('.uasset','.umap','.png')
$relativePaths=[Collections.Generic.List[string]]::new()
foreach($folder in @('Source','Config','Content','ArtSource','Bridge','Scripts')){
    $basePath=Join-Path $sourceRoot $folder
    if(Test-Path -LiteralPath $basePath){
        foreach($file in Get-ChildItem -LiteralPath $basePath -File -Recurse){
            if($extensions -contains $file.Extension.ToLowerInvariant()){
                $relativePaths.Add($file.FullName.Substring($sourceRoot.TrimEnd('\').Length+1).Replace('\','/'))
            }
        }
    }
}
foreach($file in Get-ChildItem -LiteralPath $sourceRoot -File){
    if($file.Extension -in @('.uproject','.cmd')){$relativePaths.Add($file.Name)}
}
$ordered=$relativePaths.ToArray()
[Array]::Sort($ordered,[StringComparer]::Ordinal)
$manifest=[Text.StringBuilder]::new()
$sha=[Security.Cryptography.SHA256]::Create()
try{
    foreach($relative in $ordered){
        $absolute=Join-Path $sourceRoot $relative
        $bytes=if([IO.Path]::GetExtension($relative).ToLowerInvariant() -in $binary){[IO.File]::ReadAllBytes($absolute)}else{[Text.Encoding]::UTF8.GetBytes([IO.File]::ReadAllText($absolute).Replace("`r`n","`n"))}
        $digest=[BitConverter]::ToString($sha.ComputeHash([byte[]]$bytes)).Replace('-','').ToLowerInvariant()
        [void]$manifest.Append($relative).Append([char]0).Append($digest).Append("`n")
    }
    $fingerprint=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($manifest.ToString()))).Replace('-','').ToLowerInvariant()
}finally{$sha.Dispose()}
$commit='unknown';$dirty=$false
if(Get-Command git -ErrorAction SilentlyContinue){
    # Git is optional in a source ZIP. Windows PowerShell can turn native stderr
    # into a terminating error under Stop even when redirected, so contain it here.
    $reported=$null
    try { $reported=& git -C $sourceRoot rev-parse HEAD 2>$null } catch { $reported=$null }
    if($LASTEXITCODE -eq 0 -and $reported -match '^[a-f0-9]{40}$'){
        $commit=$reported
        try {
            $changes=& git -C $sourceRoot status --porcelain -- Source Config Content ArtSource Bridge Scripts ImperialWingsSimulator.uproject ImperialWings.cmd 2>$null
            $dirty=($LASTEXITCODE -ne 0) -or -not [string]::IsNullOrWhiteSpace(($changes -join "`n"))
        } catch { $dirty=$true } # Never report a known repository as clean after a failed status query.
    }
}
$settings=[IO.File]::ReadAllText((Join-Path $sourceRoot 'Config\DefaultGame.ini'))
$version=[regex]::Match($settings,'(?m)^ProjectVersion=([^\r\n]+)').Groups[1].Value.Trim()
[pscustomobject]@{version=$version;source_commit=$commit;source_dirty=$dirty;source_sha256=$fingerprint;input_count=$ordered.Count}
