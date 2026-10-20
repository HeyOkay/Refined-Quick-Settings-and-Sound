<#
Mouse-only demo recorder/replayer. Run locally in an interactive Windows session.
Record once with the mouse; Replay repeats the saved timeline and creates a 50 fps GIF.
F8 selects crop corners / starts / stops recording. F9 aborts.
No keyboard input, audio, installation, or shell restarts are recorded or performed.
#>
[CmdletBinding()]
param(
    [ValidateSet('Record','Replay','Encode')][string]$Mode='Record',
    [string]$Profile='',
    [string]$Output='',
    [ValidateRange(128,1920)][int]$Width=640,
    [ValidateRange(5,120)][int]$MaxSeconds=45,
    [string]$Ffmpeg='ffmpeg'
)
$ErrorActionPreference='Stop'
if(!$Profile){$Profile=Join-Path $PSScriptRoot 'demo\mouse-script.json'}
if(!$Output){$Output=Join-Path $PSScriptRoot '..\docs\assets\preview.gif'}
# Replay needs the timeline created by a completed Record pass. Check before
# loading the native helper or creating a capture process.
if($Mode -eq 'Replay' -and !(Test-Path -LiteralPath $Profile -PathType Leaf)) {
    Write-Warning "No recorded mouse script found: $Profile"
    Write-Host 'First run Record, demonstrate the mod with the mouse, and press F8 to save.'
    Write-Host ('cd "'+(Split-Path -Parent $PSScriptRoot)+'"')
    Write-Host '.\tools\capture-preview.ps1 -Mode Record' -ForegroundColor Cyan
    Write-Host 'After the message "Mouse script saved", restore the initial UI state and run Replay.'
    return
}
$ffmpegCommand=Get-Command $Ffmpeg -ErrorAction Stop
$ffmpegPath=$ffmpegCommand.Source
$sessionDir=Join-Path $PSScriptRoot 'demo\local'
New-Item -ItemType Directory -Force -Path $sessionDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Profile) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Output) | Out-Null
$recordPath=Join-Path $sessionDir 'record.mp4'
$replayPath=Join-Path $sessionDir 'replay.mp4'
$logPath=Join-Path $sessionDir 'capture.log'

function Encode-Gif([string]$InputVideo) {
    if(!(Test-Path -LiteralPath $InputVideo)){throw "Video missing: $InputVideo"}
    # GIF delays have 10 ms precision: 50 fps gives an exact 20 ms per frame.
    # Stable Bayer dithering avoids crawling noise over the shell's Acrylic.
    $filter="fps=50,scale='min($Width,iw)':-1:flags=lanczos,split[a][b];[a]palettegen=stats_mode=diff:reserve_transparent=0[p];[b][p]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle"
    & $ffmpegPath -hide_banner -loglevel warning -y -i $InputVideo -filter_complex $filter -loop 0 -final_delay 2 $Output
    if($LASTEXITCODE){throw "GIF encoding failed: $LASTEXITCODE"}
    Write-Host "GIF ready: $Output" -ForegroundColor Green
    Write-Host ('Size: {0:N2} MiB; moving sections: 50 fps' -f ((Get-Item -LiteralPath $Output).Length/1MB))
}
if($Mode -eq 'Encode') {
    $inputVideo=if(Test-Path -LiteralPath $replayPath){$replayPath}else{$recordPath}
    Encode-Gif $inputVideo
    return
}
if(-not ('DemoMouse' -as [type])) {
    Add-Type -TypeDefinition (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'demo\MouseDemo.cs') -Raw)
}
[DemoMouse]::Init()
function Screen-Geometry {
    return @([DemoMouse]::GetSystemMetrics(76),[DemoMouse]::GetSystemMetrics(77),[DemoMouse]::GetSystemMetrics(78),[DemoMouse]::GetSystemMetrics(79))
}
function Start-Capture($Area,[string]$Path) {
    $captureArgs=@('-hide_banner','-loglevel','warning','-y','-f','gdigrab','-framerate','60','-draw_mouse','1','-offset_x',[string]$Area.X,'-offset_y',[string]$Area.Y,'-video_size',("{0}x{1}" -f $Area.Width,$Area.Height),'-i','desktop','-an','-c:v','libx264','-preset','ultrafast','-crf','16','-pix_fmt','yuv444p',$Path)
    $info=New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName=$ffmpegPath;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardInput=$true;$info.RedirectStandardError=$true
    if($null -ne $info.PSObject.Properties['ArgumentList']) {
        foreach($arg in $captureArgs){$info.ArgumentList.Add($arg)}
    } else {
        # Direct native argv, no shell evaluation. These arguments contain no quotes.
        foreach($arg in $captureArgs){if($arg.Contains('"')){throw 'Quotes in capture arguments are unsupported on Windows PowerShell 5.1'}}
        $info.Arguments=($captureArgs|ForEach-Object{'"'+$_+'"'}) -join ' '
    }
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo=$info
    $null=$process.Start()
    $script:captureError=$process.StandardError.ReadToEndAsync()
    Start-Sleep -Milliseconds 700
    if($process.HasExited){throw ('Capture failed: '+$script:captureError.Result)}
    return $process
}
function Stop-Capture($Process) {
    if($Process -and !$Process.HasExited) {
        $Process.StandardInput.WriteLine('q')
        $Process.StandardInput.Flush()
        if(!$Process.WaitForExit(15000)){$Process.Kill();$Process.WaitForExit();throw 'Capture did not stop cleanly'}
    }
    if($script:captureError){[IO.File]::WriteAllText($logPath,$script:captureError.Result)}
    if($Process -and $Process.ExitCode){throw "Capture exited with $($Process.ExitCode); see $logPath"}
}
$capture=$null
if($Mode -eq 'Record') {
    Write-Host 'Select a crop that includes the tallest Sound panel and the tray icons.'
    Write-Host 'Move the cursor to the TOP LEFT crop corner, then press F8 (F9 aborts).'
    $a=[DemoMouse]::WaitF8()
    Write-Host 'Move the cursor to the BOTTOM RIGHT crop corner, then press F8.'
    $b=[DemoMouse]::WaitF8()
    $area=[pscustomobject]@{X=[Math]::Min($a.X,$b.X);Y=[Math]::Min($a.Y,$b.Y);Width=[Math]::Abs($a.X-$b.X);Height=[Math]::Abs($a.Y-$b.Y)}
    if($area.Width -lt 128 -or $area.Height -lt 128){throw 'Crop too small; select at least 128 x 128 pixels'}
    $screen=Screen-Geometry
    if($area.X -lt $screen[0] -or $area.Y -lt $screen[1] -or $area.X+$area.Width -gt $screen[0]+$screen[2] -or $area.Y+$area.Height -gt $screen[1]+$screen[3]){throw 'Crop is outside the virtual screen'}
    Write-Host 'Close the panels. Prepare the initial desktop and hover over the opening tray icon.'
    Write-Host 'F8 START. Demonstrate using ONLY the mouse. F8 STOP. F9 ABORT.' -ForegroundColor Cyan
    $null=[DemoMouse]::WaitF8()
    try {
        $capture=Start-Capture $area $recordPath
        $timeline=@([DemoMouse]::Record($MaxSeconds))
        $duration=[DemoMouse]::DurationMs
    } finally { Stop-Capture $capture }
    if($timeline.Count -lt 2){throw 'No mouse actions recorded'}
    $data=[ordered]@{Version=1;Screen=$screen;Area=$area;DurationMs=$duration;LeadInMs=700;Events=$timeline}
    $data|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $Profile -Encoding utf8
    Write-Host "Mouse script saved: $Profile" -ForegroundColor Green
    Write-Host 'Restore the same initial UI state, then run -Mode Replay.'
} else {
    $data=Get-Content -LiteralPath $Profile -Raw|ConvertFrom-Json
    if($data.Version -ne 1){throw 'Unsupported mouse script version'}
    if(((Screen-Geometry) -join ',') -ne ($data.Screen -join ',')){throw 'Screen geometry changed. Re-record at the current resolution and scaling.'}
    $timeline=New-Object 'System.Collections.Generic.List[DemoEvent]'
    $previous=-1.0
    foreach($item in $data.Events) {
        if($item.Ms -lt $previous -or $item.Ms -lt 0 -or $item.Ms -gt $data.DurationMs){throw 'Invalid mouse timeline'}
        if($item.Message -notin @(0x200,0x201,0x202,0x204,0x205,0x20A)){throw 'Unsupported mouse action'}
        $previous=$item.Ms
        $event=New-Object DemoEvent
        $event.Ms=$item.Ms;$event.X=$item.X;$event.Y=$item.Y;$event.Message=$item.Message;$event.Wheel=$item.Wheel
        $timeline.Add($event)
    }
    Write-Host 'Restore the original panel state, volume and mixer state. Close the panels.'
    Write-Host 'F8 starts replay and capture. Do not touch the mouse during replay. F9 aborts.' -ForegroundColor Cyan
    $null=[DemoMouse]::WaitF8()
    try {
        $capture=Start-Capture $data.Area $replayPath
        [DemoMouse]::Replay($timeline.ToArray(),[double]$data.DurationMs)
        Start-Sleep -Milliseconds 800
    } finally { Stop-Capture $capture }
    Encode-Gif $replayPath
}
