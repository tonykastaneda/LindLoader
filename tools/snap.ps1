# Capture the screen (or the VT3 window) to a PNG. Optionally text it to the user.
# Usage: snap.ps1 -Out <png> [-Send "caption"]
param([string]$Out, [string]$Send)

Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
}
"@
[Win]::SetProcessDpiAwarenessContext([IntPtr]-4) | Out-Null   # per-monitor v2

$b = New-Object System.Drawing.Rectangle 0, 0, ([Win]::GetSystemMetrics(0)), ([Win]::GetSystemMetrics(1))
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen(0, 0, 0, 0, $b.Size)
$g.Dispose()

$bmp.Save([IO.Path]::ChangeExtension($Out, ".full.png"), [System.Drawing.Imaging.ImageFormat]::Png)

# Scale down to 1280 wide so texts stay small.
$w = 1280; $h = [int]($b.Height * $w / $b.Width)
$small = New-Object System.Drawing.Bitmap $bmp, $w, $h
$small.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose(); $small.Dispose()
Write-Output "saved $Out"

# -Send texts the image over the Mac's imsg CLI (ssh host "mbp") to the number in the
# LL_IMSG_TO environment variable.
if ($Send -and $env:LL_IMSG_TO) {
    $name = [IO.Path]::GetFileName($Out)
    scp -q -o BatchMode=yes $Out "mbp:/tmp/$name"
    $cap = $Send -replace "'", "'\''"
    ssh -o BatchMode=yes mbp "/opt/homebrew/bin/imsg send --to $env:LL_IMSG_TO --text '$cap' --file /tmp/$name --service imessage --json </dev/null"
}
