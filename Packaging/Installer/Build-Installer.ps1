[CmdletBinding()]
param([Parameter(Mandatory)][string]$BundleZip,[Parameter(Mandatory)][string]$OutputPath)
$ErrorActionPreference='Stop'
function Get-InstallerFileSHA256([string]$Path){
 $hash=[Security.Cryptography.SHA256]::Create();$stream=[IO.File]::OpenRead($Path)
 try{[BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','').ToLowerInvariant()}finally{$stream.Dispose();$hash.Dispose()}
}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$bundle=[IO.Path]::GetFullPath($BundleZip)
$output=[IO.Path]::GetFullPath($OutputPath)
if(-not(Test-Path -LiteralPath $bundle -PathType Leaf)){throw 'Portable ZIP was not found.'}
if(Test-Path -LiteralPath $output){throw 'Output already exists. Choose a new release filename.'}
if([IO.Path]::GetExtension($output) -ne '.exe'){throw 'OutputPath must end in .exe.'}
$archive=[IO.Compression.ZipFile]::OpenRead($bundle)
$sha=[Security.Cryptography.SHA256]::Create()
try {
 $entries=@($archive.Entries|Where-Object {$_.Name})
 $prefixes=@($entries|ForEach-Object {($_.FullName.Replace('\','/') -split '/',2)[0]}|Sort-Object -Unique)
 if($prefixes.Count -ne 1){throw 'Portable ZIP must have one top-level directory.'}
 $prefix=$prefixes[0]+'/'
 $map=@{}
 foreach($entry in $entries){
  $name=$entry.FullName.Replace('\','/')
  if($name.StartsWith('/') -or $name.Contains(':') -or @($name.Split('/')|Where-Object {$_ -eq '..'}).Count){throw 'Unsafe ZIP path.'}
  $relative=$name.Substring($prefix.Length)
  if($map.ContainsKey($relative)){throw 'Duplicate ZIP path.'}
  if(@($relative.Split('/')|Where-Object {$_ -in @('Saved','RadioProfile.json','PilotControls.json','PilotResponse.json','Language.txt','.git','.venv')}).Count){throw 'Private/generated data found in portable ZIP.'}
  $map[$relative]=$entry
 }
 foreach($required in @('ImperialWingsLauncher.exe','Windows/ImperialWingsSimulator.exe','Guidance/ImperialGuidance.exe','Prerequisites/vc_redist.x64.exe','release-manifest.json','SHA256SUMS.txt')){if(-not $map.ContainsKey($required)){throw ('Missing '+$required)}}
 $reader=[IO.StreamReader]::new($map['release-manifest.json'].Open(),[Text.Encoding]::UTF8)
 try{$manifest=$reader.ReadToEnd()|ConvertFrom-Json}finally{$reader.Dispose()}
 $version=[string]$manifest.version
 if($version -notmatch '^[A-Za-z0-9.+-]{1,80}$' -or $manifest.private_profiles_included -ne $false){throw 'Unsupported release metadata.'}
 $reader=[IO.StreamReader]::new($map['SHA256SUMS.txt'].Open(),[Text.Encoding]::UTF8)
 try{$lines=$reader.ReadToEnd() -split '\r?\n'}finally{$reader.Dispose()}
 $listed=@{}
 foreach($line in $lines){
  if(-not $line){continue}
  if($line -notmatch '^([a-fA-F0-9]{64})  (.+)$'){throw 'Malformed ZIP hash list.'}
  $expected=$Matches[1].ToLowerInvariant();$name=$Matches[2]
  if($listed.ContainsKey($name) -or -not $map.ContainsKey($name)){throw 'ZIP hash list duplicate/missing entry.'}
  $stream=$map[$name].Open();try{$actual=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant()}finally{$stream.Dispose()}
  if($actual -ne $expected){throw ('ZIP file checksum mismatch: '+$name)}
  $listed[$name]=$true
 }
 if($listed.Count+1 -ne $map.Count){throw 'ZIP contains unlisted files.'}
}finally{$archive.Dispose();$sha.Dispose()}
$payloadHash=(Get-InstallerFileSHA256 $bundle)
$directory=[IO.Path]::GetDirectoryName($output)
[void][IO.Directory]::CreateDirectory($directory)
$work=Join-Path $directory ('installer-build-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$payload=Join-Path $work 'payload.zip'
[IO.File]::Copy($bundle,$payload,$false)
$info=Join-Path $work 'InstallerPayloadInfo.cs'
$infoText='internal static class InstallerPayloadInfo { public const string Version="'+$version+'"; public const string Sha256="'+$payloadHash+'"; }'
[IO.File]::WriteAllText($info,$infoText,[Text.UTF8Encoding]::new($false))
$csc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if(-not(Test-Path -LiteralPath $csc)){throw '.NET Framework C# compiler is unavailable. No tool is installed automatically.'}
$source=Join-Path $PSScriptRoot 'InstallerSetup.cs'
$refs=@('/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Web.Extensions.dll','/reference:System.IO.Compression.dll','/reference:System.IO.Compression.FileSystem.dll')
$common=@('/nologo','/target:winexe','/platform:x64','/optimize+','/codepage:65001')+$refs
$uninstall=Join-Path $work 'ImperialWingsUninstall.exe'
& $csc @common '/define:UNINSTALL_ONLY' ("/out:"+$uninstall) $source $info
if($LASTEXITCODE -ne 0){throw 'Uninstaller compilation failed.'}
& $csc @common ("/resource:"+$payload+',payload.zip') ("/resource:"+$uninstall+',uninstall.exe') ("/out:"+$output) $source $info
if($LASTEXITCODE -ne 0){throw 'Installer compilation failed.'}
[pscustomobject]@{Installer=$output;Version=$version;Bytes=(Get-Item -LiteralPath $output).Length;SHA256=(Get-InstallerFileSHA256 $output);PayloadSHA256=$payloadHash;PayloadFiles=$map.Count;Signed=$false;WorkDirectory=$work}
