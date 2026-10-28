<#
.SYNOPSIS
Exercises a running ImperialWingsSimulator -game window through native Windows mouse input.
.DESCRIPTION
No additional packages are required. The script resets only the in-memory ImperialWingsSimulator
scenario, clicks the real Canvas HUD, reads ImperialWingsSimulator's exported CSV files, and writes
verification artifacts below this project's Saved directory. It does not start or
terminate the game, delete reports, edit project content, or interact with other apps.
Keep the target game window visible and do not use the mouse during this test.
#>
[CmdletBinding()]
param(
    [int]$ProcessId = 0,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\Saved\Verification'),
    [string]$ReportsDir = '',
    [switch]$CheckGuidedSetup,
    [string]$GuidedDeviceId = '1209:4F54',
    [switch]$LayoutOnly,
    [string]$ExpectedSourceSha256 = '',
    [switch]$PrepareBoxer,
    [switch]$ControllerOnly,
    [switch]$CheckResponseSetup,
    [switch]$CheckFirstFlightSetup
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$savedDirectory = [IO.Path]::GetFullPath((Join-Path $projectDirectory 'Saved'))
$expectedProject = Join-Path $projectDirectory 'ImperialWingsSimulator.uproject'
$expectedProjectPattern = [regex]::Escape($expectedProject)
$packagedProcessPattern = '^ImperialWingsSimulator(?:-Win64-(?:Development|Shipping|DebugGame|Test))?\.exe$'
$reportsDirectory = if ([string]::IsNullOrWhiteSpace($ReportsDir)) { Join-Path $savedDirectory 'Reports' } else { [IO.Path]::GetFullPath($ReportsDir) }
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $outputRoot.StartsWith($savedDirectory + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Verification output must remain inside this ImperialWingsSimulator project''s Saved directory.'
}
[void](New-Item -ItemType Directory -Path $outputRoot -Force)
$runName = 'ui-' + [DateTime]::Now.ToString('yyyyMMdd-HHmmss-fff')
$runDirectory = Join-Path $outputRoot $runName
[void](New-Item -ItemType Directory -Path $runDirectory)
$resultPath = Join-Path $outputRoot 'imperial-ui-smoke-result.json'
$runResultPath = Join-Path $runDirectory 'result.json'
$script:checks = [Collections.Generic.List[object]]::new()
$script:snapshots = [Collections.Generic.List[object]]::new()
$script:screenshots = [Collections.Generic.List[string]]::new()
$script:inputActions = [Collections.Generic.List[object]]::new()
$script:windowHandle = [IntPtr]::Zero
$previousDpiContext = [IntPtr]::Zero
$startedUtc = [DateTime]::UtcNow
$success = $false
$failure = $null
$uiSourceIdentity = $null
$targetProcessId = $ProcessId

if (-not ('ImperialWingsSimulator.NativeUi' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
namespace ImperialWingsSimulator {
    [StructLayout(LayoutKind.Sequential)] public struct UiRect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct UiPoint { public int X, Y; }
    public static class NativeUi {
        public delegate bool EnumWindowsCallback(IntPtr window, IntPtr extra);
        [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsCallback callback, IntPtr extra);
        [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
        [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr window, StringBuilder text, int maximum);
        [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window, out UiRect rectangle);
        [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr window, ref UiPoint point);
        [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
        [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr window);
        [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint from, uint to, bool attach);
        [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
        [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr window);
        [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window, int command);
        [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
        [DllImport("user32.dll")] public static extern bool GetCursorPos(out UiPoint point);
        [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint x, uint y, uint data, UIntPtr extra);
        [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
        [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
        [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    }
}
'@
}
Add-Type -AssemblyName System.Drawing

function Assert-Check([string]$Name, [bool]$Condition, [string]$Detail) {
    $script:checks.Add([ordered]@{ name = $Name; passed = $Condition; detail = $Detail })
    if (-not $Condition) { throw "UI verification failed [$Name]: $Detail" }
    Write-Host "PASS $Name - $Detail"
}

function Get-ClientBounds {
    $rectangle = [ImperialWingsSimulator.UiRect]::new()
    if (-not [ImperialWingsSimulator.NativeUi]::GetClientRect($script:windowHandle, [ref]$rectangle)) {
        throw 'The ImperialWingsSimulator game window is no longer available.'
    }
    $origin = [ImperialWingsSimulator.UiPoint]::new()
    if (-not [ImperialWingsSimulator.NativeUi]::ClientToScreen($script:windowHandle, [ref]$origin)) {
        throw 'Cannot locate the ImperialWingsSimulator client area on screen.'
    }
    $width = $rectangle.Right - $rectangle.Left
    $height = $rectangle.Bottom - $rectangle.Top
    if ($width -lt 800 -or $height -lt 500) { throw "ImperialWingsSimulator client area is too small: ${width}x${height}." }
    $scale = [Math]::Min($width / 1600.0, $height / 1000.0)
    [pscustomobject]@{
        X = $origin.X; Y = $origin.Y; Width = $width; Height = $height
        Scale = $scale; OffsetX = ($width - 1600.0 * $scale) / 2.0; OffsetY = ($height - 1000.0 * $scale) / 2.0
    }
}

function Assert-TargetForeground {
    $foregroundWindow = [ImperialWingsSimulator.NativeUi]::GetForegroundWindow()
    if ($foregroundWindow -ne $script:windowHandle) {
        [uint32]$foregroundProcess = 0
        [void][ImperialWingsSimulator.NativeUi]::GetWindowThreadProcessId($foregroundWindow, [ref]$foregroundProcess)
        $script:inputActions.Add([ordered]@{ action = 'foreground-lost'; time_utc = [DateTime]::UtcNow.ToString('o'); expected_window = $script:windowHandle.ToInt64(); actual_window = $foregroundWindow.ToInt64(); actual_process = $foregroundProcess })
        throw "ImperialWingsSimulator lost foreground focus (expected process $targetProcessId; foreground process $foregroundProcess). Input stopped to avoid clicking a different application."
    }
}

function Invoke-HudClick([double]$X, [double]$Y, [ValidateSet('Main','Radio','Guide')][string]$Space='Main') {
    Assert-TargetForeground
    $bounds = Get-ClientBounds
    if($Space -eq 'Radio'){
        $bounds.Scale=[Math]::Max($bounds.Scale,[Math]::Min(1.0,[Math]::Min(($bounds.Width-32)/960.0,($bounds.Height-32)/748.0)))
        $bounds.OffsetX=$bounds.Width/2.0-800*$bounds.Scale
        $bounds.OffsetY=$bounds.Height/2.0-500*$bounds.Scale
    } elseif($Space -eq 'Guide'){
        $bounds.Scale=[Math]::Max($bounds.Scale,[Math]::Min(1.0,($bounds.Width-32)/902.0))
        $bounds.OffsetX=$bounds.Width/2.0-757*$bounds.Scale
        $bounds.OffsetY=$bounds.Height-16-938*$bounds.Scale
    }
    $screenX = [int][Math]::Round($bounds.X + $bounds.OffsetX + $X * $bounds.Scale)
    $screenY = [int][Math]::Round($bounds.Y + $bounds.OffsetY + $Y * $bounds.Scale)
    $before = [ImperialWingsSimulator.UiPoint]::new()
    $stable=$false
    # Activation/capture restoration can briefly restore an old cursor position.
    # Require TWO stable readings before pressing; ongoing interference still stops.
    for($positionAttempt=0;$positionAttempt -lt 4 -and -not $stable;$positionAttempt++){
        Assert-TargetForeground
        if (-not [ImperialWingsSimulator.NativeUi]::SetCursorPos($screenX, $screenY)) { throw 'Cannot position the mouse over ImperialWingsSimulator.' }
        Start-Sleep -Milliseconds 70
        Assert-TargetForeground
        if (-not [ImperialWingsSimulator.NativeUi]::GetCursorPos([ref]$before)) { throw 'Cannot read the native cursor position before a HUD click.' }
        if([Math]::Abs($before.X-$screenX) -le 2 -and [Math]::Abs($before.Y-$screenY) -le 2){
            Start-Sleep -Milliseconds 70
            Assert-TargetForeground
            [void][ImperialWingsSimulator.NativeUi]::GetCursorPos([ref]$before)
            $stable=[Math]::Abs($before.X-$screenX) -le 2 -and [Math]::Abs($before.Y-$screenY) -le 2
        }
    }
    if(-not $stable){throw "The native cursor did not remain stable before the click: target ($screenX,$screenY), actual ($($before.X),$($before.Y))."}
    [ImperialWingsSimulator.NativeUi]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    $pressed = [ImperialWingsSimulator.UiPoint]::new()
    try {
        Start-Sleep -Milliseconds 65
        [void][ImperialWingsSimulator.NativeUi]::GetCursorPos([ref]$pressed)
    } finally {
        [ImperialWingsSimulator.NativeUi]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    }
    Start-Sleep -Milliseconds 180
    $script:inputActions.Add([ordered]@{ action = 'HUD click'; time_utc = [DateTime]::UtcNow.ToString('o'); logical_x = $X; logical_y = $Y; layout_space = $Space; screen_x = $screenX; screen_y = $screenY; cursor_before_down_x = $before.X; cursor_before_down_y = $before.Y; cursor_while_down_x = $pressed.X; cursor_while_down_y = $pressed.Y })
    Assert-TargetForeground
}

function Save-ClientScreenshot([string]$Name) {
    Assert-TargetForeground
    $bounds = Get-ClientBounds
    $path = Join-Path $runDirectory ($Name + '.png')
    $bitmap = [Drawing.Bitmap]::new($bounds.Width, $bounds.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.X, $bounds.Y, 0, 0, [Drawing.Size]::new($bounds.Width, $bounds.Height), [Drawing.CopyPixelOperation]::SourceCopy)
        $bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
    $script:screenshots.Add($path)
    Write-Host "Screenshot: $path"
}

function Export-FleetSnapshot([string]$Label, [switch]$ViaShortcut) {
    $known = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    if (Test-Path -LiteralPath $reportsDirectory) {
        foreach ($file in Get-ChildItem -LiteralPath $reportsDirectory -Filter '*-fleet.csv' -File) { [void]$known.Add($file.FullName) }
    }
    if ($ViaShortcut) { Invoke-NativeKey 0x74 } # F5 is a read-only export, also in the radio modal.
    else { Invoke-HudClick 694 171 } # EXPORT CSV, x=636..752.
    $deadline = [DateTime]::UtcNow.AddSeconds(12)
    $selectedFile = $null
    $rows = @()
    while ([DateTime]::UtcNow -lt $deadline) {
        Assert-TargetForeground
        if (Test-Path -LiteralPath $reportsDirectory) {
            $files = @(Get-ChildItem -LiteralPath $reportsDirectory -Filter '*-fleet.csv' -File |
                Where-Object { -not $known.Contains($_.FullName) } | Sort-Object LastWriteTimeUtc -Descending)
            if ($files.Count -gt 0) {
                try { $rows = @(Import-Csv -LiteralPath $files[0].FullName); if ($rows.Count -eq 10) { $selectedFile = $files[0]; break } }
                catch { $rows = @() }
            }
        }
        Start-Sleep -Milliseconds 100
    }
    Assert-Check "$Label export click" ($null -ne $selectedFile) 'The real EXPORT CSV button created a new report with ten aircraft.'
    $ids = @($rows | ForEach-Object { $_.id } | Sort-Object -Unique)
    Assert-Check "$Label IDs" ($ids.Count -eq 10 -and $ids[0] -eq 'IW-01' -and $ids[9] -eq 'IW-10') 'Ten unique Imperial Wings aircraft IDs are present.'
    $copyPath = Join-Path $runDirectory ($Label + '-fleet.csv')
    Copy-Item -LiteralPath $selectedFile.FullName -Destination $copyPath
    $script:snapshots.Add([ordered]@{
        label = $Label; report = $selectedFile.FullName; verificationCopy = $copyPath; rows = $rows.Count
        fleet = @($rows | ForEach-Object { [ordered]@{ id = $_.id; armed = $_.armed; autonomous = $_.autonomous; mode = $_.mode; z_m = $_.z_m; throttle = $_.throttle; sdacs_connected = $_.sdacs_connected; input_source = $_.input_source; selected = $_.selected; camera_view = $_.camera_view; radio_connected = $_.radio_connected; radio_calibrated = $_.radio_calibrated; radio_enabled = $_.radio_enabled } })
    })
    [pscustomobject]@{ Rows = $rows; FleetPath = $selectedFile.FullName }
}
function Get-Aircraft($Snapshot, [string]$Id) { @($Snapshot.Rows | Where-Object { $_.id -eq $Id })[0] }
function Get-PositionSignature($Snapshot) { (($Snapshot.Rows | Sort-Object id | ForEach-Object { "$($_.id)|$($_.x_m)|$($_.y_m)|$($_.z_m)" }) -join "`n") }
function Get-MeanAltitude($Snapshot) { ($Snapshot.Rows | ForEach-Object { [double]$_.z_m } | Measure-Object -Average).Average }
function Test-DisarmedFleet($Snapshot) { @($Snapshot.Rows | Where-Object { $_.armed -ne '0' -or $_.autonomous -ne '0' }).Count -eq 0 }
function Assert-MatchUiState($Snapshot, [string]$SelectedId, [int]$CameraView) {
    $columns = @($Snapshot.Rows[0].PSObject.Properties.Name)
    Assert-Check 'Selection and camera telemetry fields' ($columns -contains 'selected' -and $columns -contains 'camera_view') 'The current build exports selected and camera_view; screenshots alone do not establish match state.'
    $selectedRows = @($Snapshot.Rows | Where-Object { $_.selected -eq '1' -or $_.selected -eq 'true' })
    Assert-Check "Selected aircraft $SelectedId" ($selectedRows.Count -eq 1 -and $selectedRows[0].id -eq $SelectedId) "Exactly one exported aircraft is selected: expected $SelectedId."
    Assert-Check "Actual camera view $CameraView" (@($Snapshot.Rows | Where-Object { [int]$_.camera_view -ne $CameraView }).Count -eq 0) "All rows report camera view $CameraView after the actual input."
}
function Invoke-NativeKey([byte]$Key, [int]$DurationMilliseconds = 70) {
    Assert-TargetForeground
    [uint32]$extended = if ($Key -in @(0x24, 0x25, 0x26, 0x27, 0x28)) { 1 } else { 0 }
    [ImperialWingsSimulator.NativeUi]::keybd_event($Key, 0, $extended, [UIntPtr]::Zero)
    try { Start-Sleep -Milliseconds $DurationMilliseconds }
    finally { [ImperialWingsSimulator.NativeUi]::keybd_event($Key, 0, ($extended -bor 2), [UIntPtr]::Zero) }
    $script:inputActions.Add([ordered]@{ action = 'native key'; time_utc = [DateTime]::UtcNow.ToString('o'); virtual_key = $Key; held_ms = $DurationMilliseconds })
    Start-Sleep -Milliseconds 180
}
try {
    [void][ImperialWingsSimulator.NativeUi]::SetProcessDPIAware()
    try { $previousDpiContext = [ImperialWingsSimulator.NativeUi]::SetThreadDpiAwarenessContext([IntPtr](-4)) } catch { }
    if ($targetProcessId -eq 0) {
        $processes = @(Get-CimInstance Win32_Process | Where-Object {
            ($_.Name -eq 'UnrealEditor.exe' -and $_.CommandLine.Replace('/','\') -match $expectedProjectPattern -and $_.CommandLine -match '(?i)(?:^|\s)-game(?:\s|$)') -or
            ($_.Name -match $packagedProcessPattern -and $_.ExecutablePath -and $_.ExecutablePath.StartsWith($projectDirectory + '\', [StringComparison]::OrdinalIgnoreCase))
        })
        if ($processes.Count -ne 1) { throw "Expected one running ImperialWingsSimulator game process; found $($processes.Count). Pass -ProcessId explicitly." }
        $targetProcessId = [int]$processes[0].ProcessId
    }
    $processInfo = Get-CimInstance Win32_Process -Filter "ProcessId=$targetProcessId"
    if (-not $processInfo -or ($processInfo.Name -ne 'UnrealEditor.exe' -and $processInfo.Name -notmatch $packagedProcessPattern)) {
        throw 'The requested process is not UnrealEditor or ImperialWingsSimulator.'
    }
    if ($processInfo.Name -eq 'UnrealEditor.exe' -and ($processInfo.CommandLine.Replace('/','\') -notmatch $expectedProjectPattern -or $processInfo.CommandLine -notmatch '(?i)(?:^|\s)-game(?:\s|$)')) {
        throw "The requested UnrealEditor process must run $expectedProject with -game."
    }
    if ($processInfo.Name -match $packagedProcessPattern -and (-not $processInfo.ExecutablePath -or -not $processInfo.ExecutablePath.StartsWith($projectDirectory + '\', [StringComparison]::OrdinalIgnoreCase))) {
        throw 'The requested packaged executable must be inside this ImperialWingsSimulator project directory.'
    }
    if($CheckResponseSetup -or $CheckFirstFlightSetup){
        $isolatedData=Split-Path -Parent $reportsDirectory
        $argument=[regex]::Match($processInfo.CommandLine,'(?i)(?:^|\s)"-ImperialDataDir=([^"]+)"(?:\s|$)|(?:^|\s)-ImperialDataDir=([^\s"]+)(?:\s|$)')
        $runtimeData=if($argument.Groups[1].Success){$argument.Groups[1].Value}else{$argument.Groups[2].Value}
        if(-not $ControllerOnly -or -not $runtimeData -or -not $isolatedData.StartsWith($savedDirectory+'\',[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFullPath($runtimeData) -ne $isolatedData){
            throw 'Response screen tests require ControllerOnly and matching explicit isolated data inside project Saved.'
        }
        $responsePath=Join-Path $isolatedData 'Config\PilotResponse.json'
        if((Test-Path -LiteralPath $responsePath) -or (Test-Path -LiteralPath (Join-Path $isolatedData 'Config\RadioProfile.json'))){throw 'Response screen test requires new QA settings, never an existing user profile.'}
    }
    $windows = [Collections.Generic.List[object]]::new()
    $callback = [ImperialWingsSimulator.NativeUi+EnumWindowsCallback]{
        param([IntPtr]$candidate, [IntPtr]$extra)
        [uint32]$ownerId = 0
        [void][ImperialWingsSimulator.NativeUi]::GetWindowThreadProcessId($candidate, [ref]$ownerId)
        if ($ownerId -eq $targetProcessId -and [ImperialWingsSimulator.NativeUi]::IsWindowVisible($candidate)) {
            $rectangle = [ImperialWingsSimulator.UiRect]::new()
            if ([ImperialWingsSimulator.NativeUi]::GetClientRect($candidate, [ref]$rectangle)) {
                $title = [Text.StringBuilder]::new(512)
                [void][ImperialWingsSimulator.NativeUi]::GetWindowText($candidate, $title, $title.Capacity)
                $windows.Add([pscustomobject]@{ Handle = $candidate; Area = ($rectangle.Right - $rectangle.Left) * ($rectangle.Bottom - $rectangle.Top); Title = $title.ToString() })
            }
        }
        return $true
    }
    [void][ImperialWingsSimulator.NativeUi]::EnumWindows($callback, [IntPtr]::Zero)
    $largestWindow = @($windows | Sort-Object Area -Descending | Select-Object -First 1)
    if ($largestWindow.Count -eq 0 -or $largestWindow[0].Area -le 0) { throw 'No visible ImperialWingsSimulator game window was found.' }
    $script:windowHandle = $largestWindow[0].Handle
    if ([ImperialWingsSimulator.NativeUi]::IsIconic($script:windowHandle)) { [void][ImperialWingsSimulator.NativeUi]::ShowWindow($script:windowHandle, 9) }
    # The caller can be a background shell; attach only for this initial activation.
    $foregroundHandle=[ImperialWingsSimulator.NativeUi]::GetForegroundWindow()
    [uint32]$foregroundOwner=0
    $foregroundThread=[ImperialWingsSimulator.NativeUi]::GetWindowThreadProcessId($foregroundHandle,[ref]$foregroundOwner)
    $callerThread=[ImperialWingsSimulator.NativeUi]::GetCurrentThreadId()
    [void][ImperialWingsSimulator.NativeUi]::AttachThreadInput($callerThread,$foregroundThread,$true)
    try {
        [void][ImperialWingsSimulator.NativeUi]::BringWindowToTop($script:windowHandle)
        [void][ImperialWingsSimulator.NativeUi]::SetForegroundWindow($script:windowHandle)
    } finally { [void][ImperialWingsSimulator.NativeUi]::AttachThreadInput($callerThread,$foregroundThread,$false) }
    Start-Sleep -Milliseconds 350
    Assert-TargetForeground
    $initialBounds = Get-ClientBounds
    Assert-Check 'ImperialWingsSimulator native window' $true "$($largestWindow[0].Title), process $targetProcessId, client $($initialBounds.Width)x$($initialBounds.Height)."

    Invoke-HudClick 1395 232 # Select IW-01 explicitly.
    Invoke-NativeKey 0x24 # HOME resets the in-memory match.
    Invoke-NativeKey 0x31 # View 1 is the outside-court LOS pilot station.
    Start-Sleep -Milliseconds 550
    Save-ClientScreenshot '01-ready'
    $ready = Export-FleetSnapshot '01-ready'
    Assert-MatchUiState $ready 'IW-01' 1
    Assert-Check 'Reset disarms ten aircraft' (Test-DisarmedFleet $ready) 'All ten aircraft are disarmed with autonomy disabled.'
    Assert-Check 'All ten aircraft use ANGLE mode' (@($ready.Rows | Where-Object {$_.flight_mode -ne 'ANGLE' -or [double]$_.angle_limit_deg -ne 35}).Count -eq 0) 'Manual roll and pitch command a maximum 35-degree tilt with self-leveling.'
    $expectedRoles=@('STRIKER','GUIDE','LIBERO','SWEEPER','KEEPER')
    foreach($layoutRow in $ready.Rows){
        $index=[int]$layoutRow.id.Substring(3)-1
        $roleIndex=$index%5
        $expectedX=if($index -lt 5){-6.5}else{6.5}
        $roleLanes=@(0,-1.25,-2.5,1.25,2.5)
        $expectedY=if($index -lt 5){$roleLanes[$roleIndex]}else{-$roleLanes[$roleIndex]}
        $positionsMatch=[Math]::Abs([double]$layoutRow.x_m-$expectedX) -lt .03 -and [Math]::Abs([double]$layoutRow.y_m-$expectedY) -lt .03 -and
            [Math]::Abs([double]$layoutRow.home_x_m-$expectedX) -lt .001 -and [Math]::Abs([double]$layoutRow.home_y_m-$expectedY) -lt .001 -and
            [Math]::Abs([double]$layoutRow.pad_x_m-$expectedX) -lt .001 -and [Math]::Abs([double]$layoutRow.pad_y_m-$expectedY) -lt .001
        Assert-Check "Start/home/actual rendered pad $($layoutRow.id)" ($positionsMatch -and $layoutRow.role -eq $expectedRoles[$roleIndex]) "FIDA start line at x=$expectedX m; training role lane y=$expectedY m."
        $yawError=if($index -lt 5){[Math]::Abs([double]$layoutRow.yaw_deg)}else{[Math]::Abs([Math]::Abs([double]$layoutRow.yaw_deg)-180)}
        Assert-Check "Initial direction faces centre $($layoutRow.id)" ($yawError -lt .5) 'Blue faces +X; Red faces -X.'
    }
    if(-not $LayoutOnly -and -not $ControllerOnly){Assert-Check 'Actual SDACS connection' (@($ready.Rows | Where-Object { $_.sdacs_connected -ne '1' }).Count -eq 0) 'The game reports a live SDACS connection in exported telemetry.'}
    $radioColumns = @($ready.Rows[0].PSObject.Properties.Name)
    $uiSourceIdentity=& (Join-Path $projectDirectory 'Scripts\Get-SourceIdentity.ps1') -ProjectRoot $projectDirectory
    if(-not $ExpectedSourceSha256){$ExpectedSourceSha256=$uiSourceIdentity.source_sha256}
    Assert-Check 'Runtime build matches expected source inputs' ($radioColumns -contains 'build_source_sha256' -and @($ready.Rows | Where-Object { $_.build_source_sha256 -ne $ExpectedSourceSha256 }).Count -eq 0) 'All ten records identify the requested compiled source snapshot; workspace changes are separately recorded.'
    Assert-Check 'Radio telemetry fields are present' ($radioColumns -contains 'input_source' -and $radioColumns -contains 'radio_connected' -and $radioColumns -contains 'radio_calibrated' -and $radioColumns -contains 'radio_enabled') 'The build exports actual input-source and USB safety state.'
    Assert-Check 'Application starts in keyboard mode' (@($ready.Rows | Where-Object { $_.input_source -ne 'KEYBOARD' -or $_.radio_enabled -ne '0' }).Count -eq 0) 'All ten disarmed aircraft report KEYBOARD; USB activation is never automatic.'
    Invoke-NativeKey 0x43 # C opens the real USB calibration modal.
    Save-ClientScreenshot '01a-radio-device-and-axes'
    if($ControllerOnly){
        $profilePath=Join-Path (Split-Path -Parent $reportsDirectory) 'Config\RadioProfile.json'
        $controllerProfileBefore=if(Test-Path -LiteralPath $profilePath){(Get-FileHash -LiteralPath $profilePath).Hash}else{'ABSENT'}
        Invoke-HudClick 515 676 -Space Radio # Explicit saved-radio connection, never automatic ARM.
        $savedAttempt=Export-FleetSnapshot 'controller-saved-connect' -ViaShortcut
        Assert-Check 'Saved connection never arms aircraft' (Test-DisarmedFleet $savedAttempt) 'The explicit reconnect action leaves all ten motors disarmed.'
        if($controllerProfileBefore -eq 'ABSENT'){
            Assert-Check 'Missing saved profile leaves keyboard active' (@($savedAttempt.Rows|Where-Object {$_.radio_enabled -ne '0' -or $_.input_source -ne 'KEYBOARD'}).Count -eq 0) 'No profile is synthesized to satisfy reconnect.'
        }
        $diagnostics=Join-Path (Split-Path -Parent $reportsDirectory) 'Diagnostics'
        $oldReports=@(Get-ChildItem -LiteralPath $diagnostics -Filter 'radio-*.json' -ErrorAction SilentlyContinue | ForEach-Object FullName)
        Invoke-HudClick 1140 849 -Space Radio
        Start-Sleep -Milliseconds 450
        $newReport=Get-ChildItem -LiteralPath $diagnostics -Filter 'radio-*.json' | Where-Object {$_.FullName -notin $oldReports} | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        Assert-Check 'Device report exports actual Windows devices' ($null -ne $newReport) 'The real diagnostic action produced a new JSON file.'
        $deviceReport=Get-Content -LiteralPath $newReport.FullName -Raw|ConvertFrom-Json
        if($controllerProfileBefore -eq 'ABSENT' -and @($deviceReport.devices).Count -gt 0){
            Invoke-HudClick 565 342 -Space Radio # First actual device card, not a guessed Windows slot.
            $card=Export-FleetSnapshot 'controller-device-card' -ViaShortcut
            Assert-Check 'Device card selects a listed Windows device' ($card.Rows[0].radio_connected -eq '1' -and $card.Rows[0].radio_device_id -eq $deviceReport.devices[0].driver_id) 'Selected driver ID matches the first device in the Windows diagnostic list.'
            Assert-Check 'Device selection does not enable or arm' ((Test-DisarmedFleet $card) -and $card.Rows[0].radio_enabled -eq '0') 'Selecting a device does not bypass physical calibration.'
        }
        Save-ClientScreenshot 'controller-simple-view'
        Invoke-HudClick 1070 164 -Space Radio
        Save-ClientScreenshot 'controller-advanced-view'
        Invoke-HudClick 1070 164 -Space Radio
        Save-ClientScreenshot 'controller-simple-restored'
        if($CheckResponseSetup){
            Invoke-HudClick 1030 718 -Space Radio # Basic response opens without applying a preset.
            Save-ClientScreenshot 'response-basic'
            Assert-Check 'Opening basic response does not save defaults' (-not(Test-Path -LiteralPath $responsePath)) 'Custom settings are not replaced just by entering the screen.'
            Invoke-HudClick 560 338 -Space Radio # Gentle preview.
            Invoke-HudClick 1120 801 -Space Radio # Cancel.
            Assert-Check 'Cancelling basic preset creates no settings' (-not(Test-Path -LiteralPath $responsePath)) 'Native mouse selection remains a draft.'
            Invoke-HudClick 1030 718 -Space Radio
            Invoke-HudClick 560 338 -Space Radio
            Invoke-HudClick 860 801 -Space Radio # Explicit save.
            $gentle=Get-Content -LiteralPath $responsePath -Raw | ConvertFrom-Json
            Assert-Check 'Native basic save applies gentle response' ($gentle.rate_0 -eq 120 -and $gentle.rate_1 -eq 120 -and $gentle.rate_2 -eq 90 -and $gentle.expo_0 -eq .35) 'QA settings contain the chosen response, with no calibration changes.'
            Invoke-HudClick 1030 718 -Space Radio
            Invoke-HudClick 1120 210 -Space Radio # Advanced response.
            Invoke-HudClick 523 327 -Space Radio # Roll rate +15.
            Save-ClientScreenshot 'response-advanced'
            Invoke-HudClick 1120 210 -Space Radio # Basic retains the custom draft.
            Invoke-HudClick 860 801 -Space Radio
            $custom=Get-Content -LiteralPath $responsePath -Raw | ConvertFrom-Json
            Assert-Check 'Native basic-advanced switch preserves custom draft' ($custom.rate_0 -eq 135 -and $custom.rate_1 -eq 120 -and $custom.expo_0 -eq .35) 'Saving from basic retains the edit made in advanced.'
            $responseFleet=Export-FleetSnapshot 'controller-response-saved' -ViaShortcut
            Assert-Check 'Native response save keeps USB disabled and motors stopped' ((Test-DisarmedFleet $responseFleet) -and $responseFleet.Rows[0].radio_enabled -eq '0') 'Saving never activates or arms a controller.'
        }
    }
    Invoke-HudClick 109 171 # Underlying AUTO must be consumed by the modal backdrop.
    $uncalibratedAtStart = $ready.Rows[0].radio_calibrated -eq '0'
    if ($uncalibratedAtStart) {
        Invoke-HudClick 615 801 -Space Radio # ENABLE RADIO before physical calibration must fail.
        Save-ClientScreenshot '01b-uncalibrated-radio-refused'
    } else {
        $script:checks.Add([ordered]@{ name = 'Uncalibrated radio activation refusal'; passed = $null; skipped = $true; detail = 'An existing calibrated device profile was preserved; no calibration state or physical stick movement was synthesized.' })
    }
    Invoke-NativeKey 0x43 # C closes the modal, restoring ordinary Canvas hitboxes.
    $radioClosed = Export-FleetSnapshot '01c-radio-closed'
    Assert-Check 'Radio modal consumes underlying commands' (Test-DisarmedFleet $radioClosed) 'Clicking the covered AUTO button left the full fleet disarmed.'
    Assert-Check 'Closing radio panel retains keyboard mode' (@($radioClosed.Rows | Where-Object { $_.input_source -ne 'KEYBOARD' -or $_.radio_enabled -ne '0' }).Count -eq 0) 'USB remains disabled after opening and closing calibration.'
    if ($uncalibratedAtStart) {
        Assert-Check 'Uncalibrated radio activation is rejected' (@($radioClosed.Rows | Where-Object { $_.radio_enabled -ne '0' -or $_.radio_calibrated -ne '0' }).Count -eq 0) 'An explicit ENABLE RADIO attempt kept the real uncalibrated USB source disabled.'
    }
    Assert-MatchUiState $radioClosed 'IW-01' 1
    if($CheckFirstFlightSetup){
        Invoke-NativeKey 0x70 # F1 opens only the non-modal guide.
        Save-ClientScreenshot 'guide-native-ready'
        $guideOpened=Export-FleetSnapshot 'guide-native-opened' -ViaShortcut
        Assert-Check 'F1 guide opening preserves manual disarmed fleet' ((Test-DisarmedFleet $guideOpened) -and $guideOpened.Rows[0].radio_enabled -eq '0') 'Opening help does not select, arm, enable USB or change pilot input.'
        Assert-MatchUiState $guideOpened 'IW-01' 1
        Invoke-HudClick 550 912 -Space Guide # Explicit measured-guide START.
        $guideStart=Export-FleetSnapshot 'guide-native-start' -ViaShortcut
        Assert-Check 'Native measured guide start leaves ARM separate' ((Test-DisarmedFleet $guideStart) -and $guideStart.Rows[0].radio_enabled -eq '0') 'The observer starts; the pilot still controls all motors.'
        Save-ClientScreenshot 'guide-native-takeoff'
        Invoke-HudClick 460 912 -Space Guide # NEXT is disabled until measured completion.
        $guideIncomplete=Export-FleetSnapshot 'guide-native-incomplete' -ViaShortcut
        Assert-Check 'Incomplete guide NEXT cannot fly or arm' (Test-DisarmedFleet $guideIncomplete) 'An incomplete measurement does not advance or issue flight commands.'
        Invoke-HudClick 704 912 -Space Guide # Retry only this observation step.
        $guideRetry=Export-FleetSnapshot 'guide-native-retry' -ViaShortcut
        Assert-Check 'Retrying the guide step preserves motor safety' (Test-DisarmedFleet $guideRetry) 'Retry observes the current position and never flies.'
        Invoke-NativeKey 0x70 # F1 closes only guidance.
        $guideClosed=Export-FleetSnapshot 'guide-native-closed' -ViaShortcut
        Assert-Check 'F1 closes guidance without arming or enabling USB' ((Test-DisarmedFleet $guideClosed) -and $guideClosed.Rows[0].radio_enabled -eq '0') 'Closing observation does not change flight ownership.'
        Invoke-NativeKey 0x70
        Save-ClientScreenshot 'guide-native-reopened'
        Invoke-HudClick 1164 730 -Space Guide # Close button uses the same visibility-only path.
    }
    if($PrepareBoxer){
        Invoke-NativeKey 0x43
        $boxerState=Export-FleetSnapshot 'boxer-select-current' -ViaShortcut
        for($step=0;$step -lt 16 -and $boxerState.Rows[0].radio_device_id -ne '1209:4F54';$step++){
            Invoke-HudClick 1198 222 -Space Radio
            $boxerState=Export-FleetSnapshot ('boxer-select-'+$step) -ViaShortcut
        }
        Assert-Check 'Actual Boxer saved profile loaded' ($boxerState.Rows[0].radio_connected -eq '1' -and $boxerState.Rows[0].radio_device_id -eq '1209:4F54' -and $boxerState.Rows[0].radio_calibrated -eq '1') 'The real saved profile was loaded for the selected Windows identity.'
        Invoke-HudClick 615 801 -Space Radio
        Save-ClientScreenshot 'boxer-angle-profile-panel'
        Invoke-NativeKey 0x43
        $prepared=Export-FleetSnapshot 'boxer-angle-ready'
        Assert-Check 'Boxer setup leaves all aircraft disarmed' (Test-DisarmedFleet $prepared) 'Loading and enabling a radio does not arm any aircraft.'
        Save-ClientScreenshot 'boxer-angle-ready'
        if($LayoutOnly){$success=$true;return}
    }

    if ($CheckGuidedSetup) {
        # This exercises only the real device's idle/rejection/cancel path.
        # It neither substitutes stick readings nor saves a calibration profile.
        $profilePath = Join-Path (Split-Path -Parent $reportsDirectory) 'Config\RadioProfile.json'
        $profileBefore = if (Test-Path -LiteralPath $profilePath) { (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash } else { 'ABSENT' }
        Invoke-NativeKey 0x43
        $deviceState = Export-FleetSnapshot '01d-guided-device' -ViaShortcut
        for ($attempt = 0; $attempt -lt 16 -and $deviceState.Rows[0].radio_device_id -ne $GuidedDeviceId; $attempt++) {
            Invoke-HudClick 1198 222 -Space Radio # NEXT DEVICE, actual Windows enumeration only.
            $deviceState = Export-FleetSnapshot "01d-guided-device-$attempt" -ViaShortcut
        }
        Assert-Check 'Guided setup uses selected physical device' ($deviceState.Rows[0].radio_connected -eq '1' -and $deviceState.Rows[0].radio_device_id -eq $GuidedDeviceId) "Actual selected driver ID is $GuidedDeviceId."
        Invoke-HudClick 528 536 -Space Radio # Select MODE 2 for this physical Mode 2 fixture.
        Invoke-HudClick 706 536 -Space Radio # STICK SETUP / MODE 2.
        Save-ClientScreenshot '01e-guided-mode2-ready'
        $guideReady = Export-FleetSnapshot '01e-guided-ready' -ViaShortcut
        Assert-Check 'Guided setup opens at first ready step' ($guideReady.Rows[0].radio_setup_phase -eq '1' -and $guideReady.Rows[0].radio_setup_channel -eq '0') 'No channel assignment is inferred before actual physical movement.'
        Invoke-NativeKey 0x0D # ENTER starts the calibration step, never AUTO FLEET.
        Invoke-NativeKey 0x20 # SPACE must not arm while the radio modal is open.
        Invoke-NativeKey 0x26 300 # UP must not accumulate hidden keyboard throttle.
        Start-Sleep -Milliseconds 650
        $guideCapture = Export-FleetSnapshot '01f-guided-capture' -ViaShortcut
        Assert-Check 'Enter starts stick capture without launch' ($guideCapture.Rows[0].radio_setup_phase -eq '2' -and (Test-DisarmedFleet $guideCapture)) 'The setup shortcut did not launch or arm the ten-aircraft fleet.'
        Assert-Check 'Guided capture receives actual input samples' ([int]$guideCapture.Rows[0].radio_setup_samples -gt 16) 'Samples came from the selected WinMM input device.'
        Assert-Check 'Radio modal blocks keyboard throttle' ([double](Get-Aircraft $guideCapture 'IW-01').throttle -eq 0) 'The UP key did not leave hidden throttle while setting up USB.'
        Invoke-NativeKey 0x0D # Idle stick cannot establish range and direction.
        Save-ClientScreenshot '01g-guided-idle-range-refused'
        $guideRefused = Export-FleetSnapshot '01g-guided-refused' -ViaShortcut
        Assert-Check 'Stationary guide step remains unconfirmed' ($guideRefused.Rows[0].radio_setup_phase -eq '2' -and $guideRefused.Rows[0].radio_setup_channel -eq '0') 'Idle physical input was not accepted as a full stick sweep.'
        Assert-Check 'Guided setup keeps USB disabled' (@($guideRefused.Rows | Where-Object { $_.radio_enabled -ne '0' -or $_.input_source -ne 'KEYBOARD' }).Count -eq 0) 'Setup does not supply flight controls before completion.'
        Invoke-NativeKey 0x43 # CLOSE cancels the unfinished guide.
        $guideCancel = Export-FleetSnapshot '01h-guided-cancelled'
        Assert-Check 'Closing discards unfinished guide' ($guideCancel.Rows[0].radio_setup_phase -eq '0' -and (Test-DisarmedFleet $guideCancel)) 'Cancel returns to keyboard mode with all aircraft disarmed.'
        $profileAfter = if (Test-Path -LiteralPath $profilePath) { (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash } else { 'ABSENT' }
        Assert-Check 'Unfinished guide preserves existing profile file' ($profileBefore -eq $profileAfter) 'The real calibration file was neither created nor changed by this test.'
    }

    if($ControllerOnly){
        Invoke-NativeKey 0x75 # F6 begins a genuine runtime flight log; no synthetic controls.
        Start-Sleep -Milliseconds 650
        Invoke-NativeKey 0x75
        $flightFolder=Join-Path (Split-Path -Parent $reportsDirectory) 'FlightLogs'
        $flight=Get-ChildItem -LiteralPath $flightFolder -Filter '*.jsonl'|Sort-Object LastWriteTime -Descending|Select-Object -First 1
        $samples=@(Get-Content -LiteralPath $flight.FullName|ForEach-Object {$_|ConvertFrom-Json})
        Assert-Check 'F6 records start samples and stop' ($samples.Count -ge 3 -and $samples[0].event -eq 'start' -and $samples[-1].event -eq 'stop') 'The README collection workflow produced an actual JSONL file.'
        Assert-Check 'Recorded aircraft match the compiled source' (@($samples|Where-Object {$_.source_sha256 -ne $ExpectedSourceSha256 -or @($_.aircraft).Count -ne 10}).Count -eq 0) 'Every record contains ten actual aircraft and the current build fingerprint.'
        $controllerProfileAfter=if(Test-Path -LiteralPath $profilePath){(Get-FileHash -LiteralPath $profilePath).Hash}else{'ABSENT'}
        Assert-Check 'Controller inspection preserves profile on disk' ($controllerProfileBefore -eq $controllerProfileAfter) 'Device selection, advanced view, refused enable and cancelled guide do not save invented calibration.'
        $success=$true;return
    }
    for($pilotIndex=0;$pilotIndex -lt 10;$pilotIndex++){
        Invoke-HudClick 1395 (232+$pilotIndex*35)
        Invoke-NativeKey 0x31
        Start-Sleep -Milliseconds 330
        $pilotSnapshot=Export-FleetSnapshot ('layout-pilot-'+$pilotIndex)
        $pilotRow=Get-Aircraft $pilotSnapshot ('IW-'+($pilotIndex+1).ToString('00'))
        $pilotX=if($pilotIndex -lt 5){-9.5}else{9.5}
        $pilotLane=@(0,-1.25,-2.5,1.25,2.5)[$pilotIndex%5]
        $pilotY=if($pilotIndex -lt 5){$pilotLane}else{-$pilotLane}
        Assert-Check "LOS pilot station for $($pilotRow.id)" ($pilotRow.selected -eq '1' -and $pilotRow.camera_view -eq '1' -and [Math]::Abs([double]$pilotRow.pilot_x_m-$pilotX) -lt .001 -and [Math]::Abs([double]$pilotRow.pilot_y_m-$pilotY) -lt .001) "Actual outside camera x=$pilotX, y=$pilotY metres matches the selected role."
        Save-ClientScreenshot ('layout-'+$pilotRow.id+'-los')
    }
    Invoke-HudClick 1395 232
    Invoke-NativeKey 0x31
    if($LayoutOnly){$success=$true;return}

    Invoke-NativeKey 0x20 # SPACE arms the possessed IW-01 at low throttle.
    $armed = Export-FleetSnapshot '02-manual-armed'
    Assert-Check 'Keyboard Space arms selected aircraft' ((Get-Aircraft $armed 'IW-01').armed -eq '1') 'IW-01 armed through its real pawn input mapping.'
    Assert-Check 'Manual flight remains manual' ((Get-Aircraft $armed 'IW-01').autonomous -eq '0') 'The selected aircraft remains in manual control.'
    $initialZ = [double](Get-Aircraft $ready 'IW-01').z_m
    Invoke-NativeKey 0x26 1500 # UP: actual configured rate 0.35/s gives approximately 0.525 throttle.
    Start-Sleep -Seconds 2
    $manualFlight = Export-FleetSnapshot '03-manual-climb'
    $manualDrone = Get-Aircraft $manualFlight 'IW-01'
    $manualZ = [double]$manualDrone.z_m
    $manualThrottle = [double]$manualDrone.throttle
    Assert-Check 'Arrow throttle reaches flight range' ($manualThrottle -gt 0.40 -and $manualThrottle -lt 0.65) "Actual exported throttle: $manualThrottle."
    Assert-Check 'Keyboard aircraft climbs with forces' ($manualZ -gt $initialZ + 0.12) "Altitude rose from $initialZ m to $manualZ m."
    Save-ClientScreenshot '02-manual-climb'
    Invoke-NativeKey 0x20
    $disarmed = Export-FleetSnapshot '04-manual-disarmed'
    Assert-Check 'Keyboard Space disarms selected aircraft' ((Get-Aircraft $disarmed 'IW-01').armed -eq '0') 'The real SPACE input disarmed IW-01.'
    Invoke-NativeKey 0x20 # Re-arming at the retained flight throttle must be refused.
    $blockedArm = Export-FleetSnapshot '04b-high-throttle-arm-blocked'
    $blockedDrone = Get-Aircraft $blockedArm 'IW-01'
    Assert-Check 'High throttle blocks manual arming' ([double]$blockedDrone.throttle -gt 0.05 -and $blockedDrone.armed -eq '0' -and $blockedDrone.autonomous -eq '0') "SPACE at throttle $($blockedDrone.throttle) left IW-01 disarmed."
    Save-ClientScreenshot '02b-high-throttle-arm-blocked'

    Invoke-NativeKey 0x24 # HOME also clears the retained high keyboard throttle.
    Start-Sleep -Milliseconds 500
    $beforeAuto = Export-FleetSnapshot '05-before-auto'
    Invoke-HudClick 109 171 # AUTO FLEET real Canvas hitbox.
    Start-Sleep -Seconds 8
    $automatic = Export-FleetSnapshot '06-autonomous-flight'
    Assert-Check 'Auto button controls ten aircraft' (@($automatic.Rows | Where-Object { $_.autonomous -ne '1' -or $_.armed -ne '1' }).Count -eq 0) 'All ten aircraft are armed and autonomous.'
    Assert-Check 'Upstream autonomous mission mode' (@($automatic.Rows | Where-Object { $_.mode -ne 'SDACS AUTONOMOUS' -or $_.sdacs_connected -ne '1' }).Count -eq 0) 'Telemetry reports SDACS AUTONOMOUS with a live backend.'
    Assert-Check 'Autonomous positions change' ((Get-PositionSignature $automatic) -ne (Get-PositionSignature $beforeAuto)) 'Exported physical positions changed after launch.'
    $autoMean = Get-MeanAltitude $automatic
    $readyMean = Get-MeanAltitude $beforeAuto
    Assert-Check 'Autonomous fleet altitude rises' ($autoMean -gt $readyMean + 0.30) "Mean altitude rose from $readyMean m to $autoMean m."
    Save-ClientScreenshot '03-autonomous-los-pilot'

    Invoke-NativeKey 0x48 # H position hold.
    $holdStart = Export-FleetSnapshot '07-hold-start'
    Assert-Check 'H enters position hold' (@($holdStart.Rows | Where-Object { $_.mode -ne 'POSITION HOLD' }).Count -eq 0) 'The native HOLD key changed actual match mode.'
    Start-Sleep -Seconds 2
    $holdEnd = Export-FleetSnapshot '08-hold-stable'
    $maxDrop = 0.0
    foreach ($row in $holdEnd.Rows) { $drop = [double](Get-Aircraft $holdStart $row.id).z_m - [double]$row.z_m; $maxDrop = [Math]::Max($maxDrop, $drop) }
    $minimumHoldAltitude = ($holdEnd.Rows | ForEach-Object { [double]$_.z_m } | Measure-Object -Minimum).Minimum
    Assert-Check 'Position hold avoids falling' ($maxDrop -lt 0.65 -and $minimumHoldAltitude -gt 0.55) "Over two seconds maximum drop=$maxDrop m; lowest aircraft=$minimumHoldAltitude m."
    Save-ClientScreenshot '04-position-hold'

    Invoke-NativeKey 0x52 # R fleet return.
    $returning = Export-FleetSnapshot '09-returning'
    Assert-Check 'R enters fleet return' (@($returning.Rows | Where-Object { $_.mode -ne 'RETURNING' }).Count -eq 0) 'The native RETURN key changed actual match mode.'
    $returnStarted = [DateTime]::UtcNow
    $returnDeadline = $returnStarted.AddSeconds(45)
    $landed = $returning
    $returnPoll = 0
    while (-not (Test-DisarmedFleet $landed) -and [DateTime]::UtcNow -lt $returnDeadline) {
        Start-Sleep -Seconds 2
        $landed = Export-FleetSnapshot ('09-return-wait-' + $returnPoll)
        $returnPoll++
    }
    $returnDuration = ([DateTime]::UtcNow - $returnStarted).TotalSeconds
    Assert-Check 'Return lands and disarms the fleet' ((Test-DisarmedFleet $landed) -and $returnDuration -le 60) "All ten aircraft finished the return and disabled autonomy in $([Math]::Round($returnDuration, 2)) seconds (60 second bound including report IO)."
    $outsideLandingEnvelope = @($landed.Rows | Where-Object {
        $initialFloorCentre = [double](Get-Aircraft $ready $_.id).z_m
        [double]$_.z_m -gt 0.65 -or [double]$_.z_m -lt $initialFloorCentre - 0.05
    })
    Assert-Check 'Returned aircraft are near the floor' ($outsideLandingEnvelope.Count -eq 0) 'Every returned aircraft centre is below 0.65 m and no more than 0.05 m below its initial resting centre.'
    $outsideHomeEnvelope = @($landed.Rows | Where-Object {
        $homeRow = Get-Aircraft $beforeAuto $_.id
        $dx = [double]$_.x_m - [double]$homeRow.x_m
        $dy = [double]$_.y_m - [double]$homeRow.y_m
        [Math]::Sqrt($dx * $dx + $dy * $dy) -gt 0.60
    })
    Assert-Check 'Return reaches original launch pads' ($outsideHomeEnvelope.Count -eq 0) 'All ten aircraft returned within 0.60 metres horizontally of their original pad coordinates.'
    Assert-Check 'Landing restores manual match mode' (@($landed.Rows | Where-Object { $_.mode -ne 'MANUAL TRAINING' }).Count -eq 0) 'The match returned to MANUAL TRAINING after the fleet landed.'
    Save-ClientScreenshot '04b-return-landed'
    Invoke-NativeKey 0x31
    Start-Sleep -Milliseconds 450
    Save-ClientScreenshot '05-los-pilot-view'
    $los = Export-FleetSnapshot '09b-los-pilot-camera'
    Assert-MatchUiState $los 'IW-01' 1
    Invoke-NativeKey 0x32
    Start-Sleep -Milliseconds 450
    Save-ClientScreenshot '06-observer-chase-view'
    $chase = Export-FleetSnapshot '09c-chase-camera'
    Assert-MatchUiState $chase 'IW-01' 2
    Invoke-NativeKey 0x33
    Start-Sleep -Milliseconds 450
    Save-ClientScreenshot '07-arena-view'
    $arena = Export-FleetSnapshot '09d-arena-camera'
    Assert-MatchUiState $arena 'IW-01' 3
    Invoke-HudClick 1395 547 # Last roster row: CSV IW-10, HUD team label WG-05.
    Save-ClientScreenshot '08-selected-IW10-team-WG05'
    $selectedLast = Export-FleetSnapshot '10-selected-last-row'
    Assert-MatchUiState $selectedLast 'IW-10' 3

    Invoke-NativeKey 0x24
    Start-Sleep -Milliseconds 550
    $reset = Export-FleetSnapshot '11-reset'
    Assert-MatchUiState $reset 'IW-10' 3
    Assert-Check 'Final reset disarms all aircraft' (Test-DisarmedFleet $reset) 'All ten aircraft are disarmed and autonomy is disabled after HOME reset.'
    Assert-Check 'Reset restores manual mode' (@($reset.Rows | Where-Object { $_.mode -ne 'MANUAL TRAINING' }).Count -eq 0) 'The reset returned MANUAL TRAINING mode.'
    Save-ClientScreenshot '09-reset-verified'

    Invoke-HudClick 109 171
    Start-Sleep -Seconds 8
    Invoke-NativeKey 0x48
    Invoke-NativeKey 0x33
    Start-Sleep -Milliseconds 350
    $overview = Export-FleetSnapshot '12-final-overview'
    Assert-MatchUiState $overview 'IW-10' 3
    Assert-Check 'Final overview holds airborne fleet' (@($overview.Rows | Where-Object { $_.armed -ne '1' -or $_.autonomous -ne '1' -or $_.mode -ne 'POSITION HOLD' }).Count -eq 0 -and (Get-MeanAltitude $overview) -gt 0.60) 'The game is left in ARENA view with the physical fleet holding in the air.'
    Save-ClientScreenshot '10-final-overview'
    $success = $true
} catch {
    $failure = $_.Exception.Message
    if ($script:windowHandle -ne [IntPtr]::Zero) {
        try { Save-ClientScreenshot '99-failure' } catch { }
    }
} finally {
    if ($previousDpiContext -ne [IntPtr]::Zero) {
        [void][ImperialWingsSimulator.NativeUi]::SetThreadDpiAwarenessContext($previousDpiContext)
    }
    $result = [ordered]@{
        ok = $success; started_utc = $startedUtc.ToString('o'); finished_utc = [DateTime]::UtcNow.ToString('o')
        process_id = $targetProcessId; window_handle = $script:windowHandle.ToInt64(); failure = $failure
        method = 'Native user32 input and client-area System.Drawing screenshots; exported CSV assertions.'
        scope = 'Only ImperialWingsSimulator in-memory scenario commands, its Saved/Reports exports, and Saved/Verification artifacts were touched.'
        project = $projectDirectory; report_directory = $reportsDirectory; artifact_directory = $runDirectory
        fps_validation = 'Manager FPS is visible in client screenshots; no numerical frame benchmark is claimed.'
        radio_validation = 'Actual device connection and calibration flags are exported; no physical USB stick movement or calibration ranges are synthesized by this script.'
        build_identity = $uiSourceIdentity
        expected_runtime_source_sha256 = $ExpectedSourceSha256
        workspace_matches_runtime = $uiSourceIdentity -and $uiSourceIdentity.source_sha256 -eq $ExpectedSourceSha256
        layout_only = [bool]$LayoutOnly
        prepare_boxer = [bool]$PrepareBoxer
        controller_only = [bool]$ControllerOnly
        camera_validation = 'View 1 is an outside-court LOS pilot station, view 2 an external training observer, and view 3 the arena overview; actual camera_view and selection fields are asserted after native input.'
        checks = @($script:checks.ToArray()); snapshots = @($script:snapshots.ToArray()); screenshots = @($script:screenshots.ToArray()); input_actions = @($script:inputActions.ToArray())
    }
    $json = $result | ConvertTo-Json -Depth 10
    [IO.File]::WriteAllText($resultPath, $json, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($runResultPath, $json, [Text.UTF8Encoding]::new($false))
    Write-Host "UI verification result: $resultPath"
}
if (-not $success) { throw $failure }
Get-Content -LiteralPath $resultPath -Raw
