# Helpers for the linked play and command line tests: Lindy through UI Automation, the virtual pads
# (vpadd.py, started with VPADS=4), and screenshots of the cabinet windows.
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class LT {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  public struct RECT { public int L, T, R, B; }
}
'@
[LT]::SetProcessDPIAware() | Out-Null # screenshots in physical pixels

$Root = (Resolve-Path "$PSScriptRoot\..\..").Path
$Tools = "$Root\tools"
$Lindy = "$Root\Lindy.exe"
$Shots = "$Tools\shots"
New-Item -ItemType Directory -Force $Shots | Out-Null
$A = [System.Windows.Automation.AutomationElement]; $S = [System.Windows.Automation.TreeScope]
$CT = [System.Windows.Automation.ControlType]

function Cond($p, $v) { New-Object System.Windows.Automation.PropertyCondition($p, $v) }
function Both($a, $b) { New-Object System.Windows.Automation.AndCondition($a, $b) }
function Invoke($e) { $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke() }
# The top-level windows of a process (Lindy's main window, its dialogs and message boxes).
function Windows($id) { $A::RootElement.FindAll($S::Children, (Cond $A::ProcessIdProperty $id)) }

# The first library folder in Lindy.ini (where the test games are looked for).
function LibraryFolder {
    $line = Get-Content "$Root\Lindy.ini" -ErrorAction SilentlyContinue | Select-String '^folders=' | Select-Object -First 1
    if (-not $line) { throw 'No library folder in Lindy.ini: add one in Lindy first.' }
    return (($line.Line -replace '^folders=', '') -split ',')[0].Trim().Trim('"') -replace '/', '\'
}

# Queues virtual pad commands ("<pad> tap START", "sleep 1", ...) and waits until vpadd.py has run them.
function Pad([string[]]$cmds) {
    foreach ($c in $cmds) {
        for ($t = 0; $t -lt 50; $t++) { try { Add-Content "$Tools\cmd.txt" $c -ErrorAction Stop; break } catch { Start-Sleep -Milliseconds 50 } }
    }
    $n = (Get-Content "$Tools\cmd.txt").Count
    for ($i = 0; $i -lt 900; $i++) {
        $done = @(Get-Content "$Tools\vpadd.log" -ErrorAction SilentlyContinue | Where-Object { $_ -notmatch 'daemon started|error' }).Count
        if ($done -ge $n) { break }; Start-Sleep -Milliseconds 200
    }
}
# One command on pads 1..$Count at once.
function Each([string]$cmd, [int]$Count) { Pad (1..$Count | ForEach-Object { "$_ $cmd" }) }

function Assert-Pads([int]$Count) {
    $py = Get-CimInstance Win32_Process -Filter "name='python.exe'" | Where-Object { $_.CommandLine -match 'vpadd\.py' }
    if (-not $py) { throw "vpadd.py isn't running: start it with VPADS=$Count (see tools\README.md)." }
}

# The running cabinets (linuxloader.exe), oldest first: cabinet 1 is the first one started.
function Cabinets { @(Get-Process linuxloader -ErrorAction SilentlyContinue | Sort-Object StartTime) }

function Snap($p, [string]$file) {
    $p.Refresh(); $h = $p.MainWindowHandle
    $r = New-Object LT+RECT; [LT]::GetWindowRect($h, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc(); [LT]::PrintWindow($h, $dc, 2) | Out-Null; $g.ReleaseHdc($dc)
    $bmp.Save($file); $g.Dispose(); $bmp.Dispose()
}
# Every cabinet's window, and one picture of them all in a 2x2 grid: shots\<name>.png.
function SnapAll([string]$name) {
    $cabs = Cabinets; $files = @()
    for ($i = 0; $i -lt $cabs.Count; $i++) { $f = "$Shots\$name-$($i + 1).png"; Snap $cabs[$i] $f; $files += $f }
    if (-not $files) { return }
    $imgs = $files | ForEach-Object { [System.Drawing.Image]::FromFile($_) }
    $w = $imgs[0].Width; $h = $imgs[0].Height
    $bmp = New-Object System.Drawing.Bitmap ($w * 2), ($h * [math]::Ceiling($imgs.Count / 2))
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    for ($i = 0; $i -lt $imgs.Count; $i++) { $g.DrawImage($imgs[$i], ($i % 2) * $w, [math]::Floor($i / 2) * $h, $w, $h) }
    $bmp.Save("$Shots\$name.png"); $g.Dispose(); $bmp.Dispose(); $imgs | ForEach-Object { $_.Dispose() }
}

# Message boxes: the text of a window, and pressing its button.
function Texts($w) { ($w.FindAll($S::Descendants, (Cond $A::ControlTypeProperty $CT::Text)) | ForEach-Object { $_.Current.Name }) -join ' | ' }
function PressButton($w) { $b = $w.FindFirst($S::Descendants, (Cond $A::ControlTypeProperty $CT::Button)); if ($b) { Invoke $b } }
