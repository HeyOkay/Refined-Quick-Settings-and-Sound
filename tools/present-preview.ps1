<#
Automatic high-quality preview. Calibrated for 2560x1440 at 125%, five mixer rows.
Records RGB losslessly, preserves native system ANI animation, exports 50 fps GIF and lossless WebP.
Requires FFmpeg, Python 3 and Pillow. F9 during movement aborts.
#>
[CmdletBinding()]
param([string]$Output='')
$ErrorActionPreference='Stop'
if(!$Output){$Output=Join-Path $PSScriptRoot '..\docs\assets\preview.gif'}
$takeDir=Join-Path $PSScriptRoot 'demo\local'
New-Item -ItemType Directory -Force -Path $takeDir,(Split-Path -Parent $Output) | Out-Null
& python -c 'from PIL import Image'
if($LASTEXITCODE){throw 'Python 3 with Pillow is required'}
Add-Type -AssemblyName System.Drawing
if(-not ('DemoInput' -as [type])) {
Add-Type -TypeDefinition @"
using System;
using System.Threading;
using System.Diagnostics;
using System.Runtime.InteropServices;
public static class DemoInput {
 [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 public static void Init(){SetThreadDpiAwarenessContext(new IntPtr(-4));if(GetSystemMetrics(0)!=2560||GetSystemMetrics(1)!=1440)throw new InvalidOperationException("This calibrated demo requires a 2560x1440 primary display at 125% scale. Recalibrate coordinates for other layouts.");}
 [StructLayout(LayoutKind.Sequential)] struct POINT{public int X,Y;}
 [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT p);
 [DllImport("user32.dll")] static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] static extern void mouse_event(uint f,uint x,uint y,uint d,UIntPtr e);
 [DllImport("user32.dll")] static extern short GetAsyncKeyState(int k);
 [DllImport("user32.dll")] static extern int GetSystemMetrics(int n);
 static void Put(int x,int y){int l=GetSystemMetrics(76),t=GetSystemMetrics(77),w=GetSystemMetrics(78),h=GetSystemMetrics(79);mouse_event(0xC001u,(uint)Math.Round((x-l)*65535.0/(w-1)),(uint)Math.Round((y-t)*65535.0/(h-1)),0,UIntPtr.Zero);}
 public static void Move(int x,int y,int ms=500,bool arc=true){POINT p;GetCursorPos(out p);var s=Stopwatch.StartNew();while(s.ElapsedMilliseconds<ms){if((GetAsyncKeyState(0x78)&0x8000)!=0)throw new OperationCanceledException("F9");double t=Math.Min(1,s.Elapsed.TotalMilliseconds/ms);t=t*t*t*(t*(t*6-15)+10);double dx=x-p.X,dy=y-p.Y,len=Math.Sqrt(dx*dx+dy*dy),bend=arc&&len>0?Math.Min(14,len*.12)*Math.Sin(Math.PI*t):0;Put((int)Math.Round(p.X+dx*t-(len>0?dy/len:0)*bend),(int)Math.Round(p.Y+dy*t+(len>0?dx/len:0)*bend));Thread.Sleep(5);}Put(x,y);}
 static Thread tracker;static volatile bool tracking;static System.Collections.Generic.List<string> positions;
 public static void StartTracking(){positions=new System.Collections.Generic.List<string>();tracking=true;tracker=new Thread(()=>{SetThreadDpiAwarenessContext(new IntPtr(-4));while(tracking){POINT p;GetCursorPos(out p);long us=(DateTime.UtcNow.Ticks-621355968000000000L)/10;positions.Add(us+","+p.X+","+p.Y);Thread.Sleep(4);}});tracker.IsBackground=true;tracker.Start();}
 public static string StopTracking(){tracking=false;if(tracker!=null)tracker.Join();return string.Join("\n",positions);}
 public static void Button(bool down){mouse_event(down?2u:4u,0,0,0,UIntPtr.Zero);}
 public static void Click(int x,int y){Move(x,y);Button(true);Thread.Sleep(35);Button(false);}
 public static void Drag(int x,int y,int tx,int ty){Move(x,y);Button(true);try{Thread.Sleep(30);Move(tx,ty,1300,false);}finally{Button(false);}}
}
"@
}
[DemoInput]::Init()
$ErrorActionPreference='Stop'

function Shot([string]$name){
 $bmp=New-Object System.Drawing.Bitmap 490,1050
 $g=[System.Drawing.Graphics]::FromImage($bmp);$g.CopyFromScreen(2070,390,0,0,$bmp.Size)
 $bmp.Save((Join-Path $takeDir ($name+'.png')));$g.Dispose();$bmp.Dispose()
}
function Pause([int]$ms){Start-Sleep -Milliseconds $ms}
# Normalize the UI and requested starting levels before recording.
[DemoInput]::Click(2020,1330);Pause 350
[DemoInput]::Click(2307,1410);Pause 850
[DemoInput]::Click(2461,1270);Pause 600
[DemoInput]::Click(2234,1317);Pause 500
[DemoInput]::Click(2499,1318);Pause 950
[DemoInput]::Click(2320,1041);Pause 500
[DemoInput]::Click(2225,998);Pause 600
Shot 'setup-collapsed'
[DemoInput]::Click(2020,1330);Pause 400
[DemoInput]::Move(2307,1410,400)
$ffmpeg=(Get-Command ffmpeg -ErrorAction Stop).Source
$video=Join-Path $takeDir 'presentation-hq.mkv'
$info=New-Object System.Diagnostics.ProcessStartInfo
$info.FileName=$ffmpeg;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
$info.RedirectStandardInput=$true;$info.RedirectStandardError=$true
$info.Arguments='-hide_banner -loglevel info -y -copyts -f gdigrab -use_wallclock_as_timestamps 1 -framerate 60 -draw_mouse 0 -offset_x 2070 -offset_y 390 -video_size 490x1050 -i desktop -vf showinfo=checksum=0,setpts=PTS-STARTPTS -an -c:v libx264rgb -preset ultrafast -crf 0 -pix_fmt rgb24 "'+$video+'"'
$p=New-Object System.Diagnostics.Process;$p.StartInfo=$info;[DemoInput]::StartTracking();$null=$p.Start()
$err=$p.StandardError.ReadToEndAsync();Pause 800
if($p.HasExited){throw $err.Result}
$clock=[Diagnostics.Stopwatch]::StartNew()
$marks=New-Object 'System.Collections.Generic.List[object]'
function Mark([string]$name){$marks.Add([pscustomobject]@{Name=$name;Seconds=[Math]::Round($clock.Elapsed.TotalSeconds,3)});Write-Output $name;Shot $name}
try {
 [DemoInput]::Click(2307,1410);Pause 450;Mark '01-control-center'
 [DemoInput]::Drag(2234,1317,2377,1317);Mark '02-volume-70'
 [DemoInput]::Drag(2377,1317,2234,1317);Mark '03-volume-20'
 [DemoInput]::Drag(2461,1270,2160,1270);Mark '04-brightness-zero'
 [DemoInput]::Drag(2176,1270,2475,1270);Mark '05-brightness-100'
 [DemoInput]::Move(2141,1270,500)
 [DemoInput]::Button($true);Pause 35;[DemoInput]::Button($false)
 [DemoInput]::Move(2161,1290,170,$false)
 Pause 160
 [DemoInput]::Move(2141,1270,170)
 [DemoInput]::Button($true);Pause 35;[DemoInput]::Button($false)
 [DemoInput]::Move(2161,1290,170,$false);Mark '06-night-light-twice'
 [DemoInput]::Click(2499,1318);Pause 450;Mark '07-sound'
 [DemoInput]::Click(2310,931);Pause 450;Mark '08-output-open'
 [DemoInput]::Click(2310,931);Pause 180;Mark '09-output-closed'
 [DemoInput]::Click(2225,1234);Pause 450;Mark '10-mixer-open'
 [DemoInput]::Drag(2320,1041,2160,1041);Mark '11-system-zero'
 [DemoInput]::Drag(2176,1041,2320,1041);Mark '12-system-50'
 [DemoInput]::Click(2499,1041);Pause 200;Mark '13-system-right-button'
 [DemoInput]::Move(2519,1061,300);Pause 400
} finally {
 [DemoInput]::Button($false)
 [IO.File]::WriteAllText((Join-Path $takeDir 'cursor.csv'),[DemoInput]::StopTracking())
 if(!$p.HasExited){$p.StandardInput.WriteLine('q');$p.StandardInput.Flush();if(!$p.WaitForExit(12000)){$p.Kill();$p.WaitForExit()}}
 [IO.File]::WriteAllText((Join-Path $takeDir 'capture.log'),$err.Result)
 $marks|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $takeDir 'marks.json') -Encoding utf8
}
if($p.ExitCode){throw "Capture failed: $($p.ExitCode)"}
Write-Output ('VIDEO SAVED: '+$video)

& python (Join-Path $PSScriptRoot 'demo\render-cursor.py') $takeDir
if($LASTEXITCODE){throw 'Cursor rendering failed'}
& python (Join-Path $PSScriptRoot 'demo\encode-hq-gif.py') $takeDir $Output
if($LASTEXITCODE){throw 'GIF encoding failed'}
$webp=[IO.Path]::ChangeExtension($Output,$null).TrimEnd('.')+'-lossless.webp'
& $ffmpeg -hide_banner -loglevel error -y -i (Join-Path $takeDir 'presentation-final.mkv') -c:v libwebp_anim -lossless 1 -compression_level 4 -q:v 100 -loop 0 $webp
if($LASTEXITCODE){throw 'WebP encoding failed'}
Write-Host ('GIF saved: '+$Output) -ForegroundColor Green
Write-Host ('Lossless WebP saved: '+$webp) -ForegroundColor Green
