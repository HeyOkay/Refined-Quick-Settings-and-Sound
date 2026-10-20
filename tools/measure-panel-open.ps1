<#
  Baseline of the native panel opening delay (works the same with the mod on or off).

  For every run the script closes the panel, presses the hotkey, and polls a few
  screen lines in the panel area until the picture stops changing:
    firstMs  - time from the key press to the first changed pixel (panel starts to appear)
    settleMs - time to the last change before the picture stays still for -QuietMs
               (slide-in animation and late content included)

  Usage (normal PowerShell window, the panel opens on the primary monitor):
    powershell -ExecutionPolicy Bypass -File tools\measure-panel-open.ps1 -Panel Quick
    powershell -ExecutionPolicy Bypass -File tools\measure-panel-open.ps1 -Panel Sound -Runs 30
    ... -RestartShellHost   restart ShellHost first (AGENTS.md: do it before every timing series)

  Panel: Quick = Win+A (Quick Settings), Sound = Win+Ctrl+V (Sound page).
  Do not touch the mouse or keyboard while it runs. Results go to the console and a CSV
  next to this script.
#>
param(
  [ValidateSet('Quick','Sound')][string]$Panel = 'Quick',
  [int]$Runs = 20,
  [int]$QuietMs = 300,
  [int]$TimeoutMs = 3000,
  [int]$PauseMs = 1200,
  [switch]$RestartShellHost,
  [string]$Label = ''
)

$ErrorActionPreference = 'Stop'

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Threading;

public static class PanelProbe {
  [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] static extern int GetSystemMetrics(int i);
  [DllImport("user32.dll")] static extern bool SystemParametersInfo(int a, int b, ref RECT r, int c);
  [DllImport("user32.dll")] static extern uint GetGuiResources(IntPtr process, uint flags);
  [DllImport("winmm.dll")] static extern uint timeBeginPeriod(uint p);
  [DllImport("winmm.dll")] static extern uint timeEndPeriod(uint p);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }

  const uint UP = 2;
  static Rectangle[] lines;

  public static string Init() {
    SetProcessDPIAware();
    RECT wa = new RECT();
    SystemParametersInfo(0x30, 0, ref wa, 0); // SPI_GETWORKAREA, physical px
    int w = wa.Right - wa.Left, h = wa.Bottom - wa.Top;
    // Panel area: right part of the work area, above the taskbar.
    int x0 = wa.Right - Math.Min(w, Math.Max(420, w * 26 / 100));
    int x1 = wa.Right - 24;
    int yBottom = wa.Bottom - 16;
    int yTop = wa.Bottom - Math.Min(h, Math.Max(500, h * 70 / 100));
    // Three horizontal lines (low, middle, high) and two vertical lines.
    lines = new Rectangle[] {
      new Rectangle(x0, yBottom - 40, x1 - x0, 1),
      new Rectangle(x0, (yTop + yBottom) / 2, x1 - x0, 1),
      new Rectangle(x0, yTop + 40, x1 - x0, 1),
      new Rectangle(x0 + (x1 - x0) / 3, yTop, 1, yBottom - yTop),
      new Rectangle(x0 + (x1 - x0) * 2 / 3, yTop, 1, yBottom - yTop),
    };
    return string.Format("area x={0}..{1} y={2}..{3}", x0, x1, yTop, yBottom);
  }

  static long Hash() {
    long hsh = 1469598103934665603L;
    foreach (var r in lines) {
      using (var bmp = new Bitmap(r.Width, r.Height, PixelFormat.Format32bppRgb)) {
        using (var g = Graphics.FromImage(bmp)) g.CopyFromScreen(r.X, r.Y, 0, 0, r.Size);
        var d = bmp.LockBits(new Rectangle(0, 0, r.Width, r.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppRgb);
        int n = d.Stride * d.Height;
        byte[] buf = new byte[n];
        Marshal.Copy(d.Scan0, buf, 0, n);
        bmp.UnlockBits(d);
        for (int i = 0; i < n; i += 4) {
          // Ignore tiny differences (dithering, cursor blink in other windows is outside the area).
          int v = (buf[i] >> 3) | ((buf[i + 1] >> 3) << 5) | ((buf[i + 2] >> 3) << 10);
          hsh = (hsh ^ v) * 1099511628211L;
        }
      }
    }
    return hsh;
  }

  [DllImport("user32.dll")] static extern uint MapVirtualKey(uint code, uint type);
  static void Down(byte k) { keybd_event(k, (byte)MapVirtualKey(k, 0), 0, UIntPtr.Zero); }
  static void Up(byte k) { keybd_event(k, (byte)MapVirtualKey(k, 0), UP, UIntPtr.Zero); }
  static void Keys(params byte[] vks) {
    foreach (var k in vks) { Down(k); Thread.Sleep(30); }
    for (int i = vks.Length - 1; i >= 0; i--) { Up(vks[i]); Thread.Sleep(10); }
  }

  public static void Close() { Keys(0x1B); }

  // GDI and USER objects of all ShellHost processes (to compare mod on/off).
  public static string Gui() {
    uint gdi = 0, usr = 0;
    foreach (var p in Process.GetProcessesByName("ShellHost")) {
      try { gdi += GetGuiResources(p.Handle, 0); usr += GetGuiResources(p.Handle, 1); } catch { }
    }
    return gdi + "/" + usr;
  }

  // Returns {firstMs, settleMs, captures}; -1 when nothing changed before the timeout.
  public static double[] Measure(bool sound, int quietMs, int timeoutMs, int pauseMs) {
    timeBeginPeriod(1);
    try {
      Close();
      Thread.Sleep(pauseMs);
      // Wait until the closed screen is still.
      long before = Hash();
      for (int i = 0; i < 20; i++) { Thread.Sleep(50); long h = Hash(); if (h == before) break; before = h; }

      // Keys go down like a person's (30 ms apart) and are released 60 ms after the
      // last one; time 0 is the last key going down, when the shortcut fires.
      byte[] keys = sound ? new byte[] { 0x5B, 0x11, 0x56 } : new byte[] { 0x5B, 0x41 };
      for (int i = 0; i < keys.Length - 1; i++) { Down(keys[i]); Thread.Sleep(30); }
      var sw = Stopwatch.StartNew(); Down(keys[keys.Length - 1]); bool released = false;
      double first = -1, last = -1; long prev = before; int caps = 0;
      while (sw.ElapsedMilliseconds < timeoutMs) {
        if (!released && sw.ElapsedMilliseconds >= 60) { for (int i = keys.Length - 1; i >= 0; i--) Up(keys[i]); released = true; }
        long h = Hash(); caps++;
        double t = sw.Elapsed.TotalMilliseconds;
        if (h != prev) { if (first < 0) first = t; last = t; prev = h; }
        else if (first >= 0 && t - last >= quietMs) break;
      }
      if (!released) for (int i = keys.Length - 1; i >= 0; i--) Up(keys[i]);
      Close();
      return new double[] { first, last, caps };
    } finally { timeEndPeriod(1); }
  }
}
'@

function Stat($values, $p) {
  $s = @($values | Sort-Object)
  if ($s.Count -eq 0) { return [double]::NaN }
  $i = [Math]::Min($s.Count - 1, [Math]::Max(0, [int][Math]::Ceiling($p * $s.Count) - 1))
  return [Math]::Round($s[$i], 0)
}

if ($RestartShellHost) {
  Write-Host 'Restarting ShellHost...'
  Get-Process ShellHost -ErrorAction SilentlyContinue | Stop-Process -Force
  Start-Sleep -Seconds 4
}

$area = [PanelProbe]::Init()
Write-Host "Panel=$Panel runs=$Runs $area"
Write-Host 'Warm-up run (not counted)...'
[void][PanelProbe]::Measure($Panel -eq 'Sound', $QuietMs, $TimeoutMs, $PauseMs)

$rows = @()
for ($i = 1; $i -le $Runs; $i++) {
  $r = [PanelProbe]::Measure($Panel -eq 'Sound', $QuietMs, $TimeoutMs, $PauseMs)
  Start-Sleep -Milliseconds 300
  $gui = [PanelProbe]::Gui()
  $row = [pscustomobject]@{ run = $i; firstMs = [Math]::Round($r[0], 1); settleMs = [Math]::Round($r[1], 1); captures = [int]$r[2]; gdiUser = $gui }
  $rows += $row
  Write-Host ("{0,3}  first {1,7} ms   settle {2,7} ms   ({3} captures)   gdi/user {4}" -f $i, $row.firstMs, $row.settleMs, $row.captures, $gui)
}
Start-Sleep -Milliseconds 300

$ok = @($rows | Where-Object { $_.firstMs -ge 0 })
$missed = $rows.Count - $ok.Count
$first = $ok | ForEach-Object firstMs
$settle = $ok | ForEach-Object settleMs
$avgCap = if ($ok.Count) { ($ok | Measure-Object captures -Average).Average } else { 0 }
$step = if ($avgCap) { [Math]::Round((($ok | Measure-Object settleMs -Average).Average + $QuietMs) / $avgCap, 1) } else { 0 }

Write-Host ''
Write-Host ("first : median {0} ms  p90 {1} ms  min {2} ms  max {3} ms" -f (Stat $first .5), (Stat $first .9), (Stat $first 0), (Stat $first 1))
Write-Host ("settle: median {0} ms  p90 {1} ms  min {2} ms  max {3} ms" -f (Stat $settle .5), (Stat $settle .9), (Stat $settle 0), (Stat $settle 1))
Write-Host ("sampling step ~{0} ms, missed runs {1}" -f $step, $missed)

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$name = "panel-open-$Panel" + $(if ($Label) { "-$Label" } else { '' }) + "-$stamp.csv"
$csv = Join-Path $PSScriptRoot $name
$rows | Export-Csv -NoTypeInformation -Path $csv
Write-Host "CSV: $csv"
