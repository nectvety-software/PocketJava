param(
 [string]$Vxp = "C:\Users\doxuanhop\Documents\LuaS30 Projects\PocketJava\build\PocketJava.vxp",
 [string]$Exe = "D:\MRE\LuaS30-IDE\emulator\VXPEmu.exe",
 [string]$Out = "C:\Users\doxuanhop\Documents\LuaS30 Projects\PocketJava\build\vxpemu-test"
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$p = Start-Process -FilePath $Exe -ArgumentList @('"'+$Vxp+'"','--autostart','--testapi','--screen-only') -PassThru
$sw=[Diagnostics.Stopwatch]::StartNew()
for($i=0;$i -lt 60;$i++) { Start-Sleep -Milliseconds 100; $p.Refresh(); if($p.MainWindowHandle -ne [IntPtr]::Zero){break} }
$sw.Stop()
if($p.MainWindowHandle -eq [IntPtr]::Zero){ throw 'VXPEmu window did not appear' }
Start-Sleep -Milliseconds 1200
Add-Type -AssemblyName System.Drawing
$r=New-Object RECTCAP
[CAP]::GetWindowRect($p.MainWindowHandle,[ref]$r)|Out-Null
$bmp=New-Object System.Drawing.Bitmap ($r.R-$r.L),($r.B-$r.T)
$g=[System.Drawing.Graphics]::FromImage($bmp);$hdc=$g.GetHdc();$null=[CAP]::PrintWindow($p.MainWindowHandle,$hdc,2);$g.ReleaseHdc($hdc);$g.Dispose()
$shot=Join-Path $Out 'startup.png';$bmp.Save($shot,[System.Drawing.Imaging.ImageFormat]::Png);$bmp.Dispose()
"PID=$($p.Id) WINDOW_MS=$($sw.ElapsedMilliseconds) SCREEN=$shot"
Add-Type @"
using System; using System.Runtime.InteropServices;
[StructLayout(LayoutKind.Sequential)] public struct RECTCAP { public int L,T,R,B; }
public class CAP { [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECTCAP r); [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint f); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h); [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l); }
"@
