# Sega Race TV linked play from Lindy: Game > Start Linked Play... with -Cabinets cabinets, then the
# virtual pads coin up, pick Multi-Player, choose cars and race. Screenshots go to shots\<Tag>-*.png;
# each cabinet keeps a warnings-only log in logs\linktest\<Tag>-<pid>.log (crash reports land there).
# Needs vpadd.py running with at least -Cabinets pads (VPADS=4). Leaves the cabinets running.
param([int]$Cabinets = 4, [string]$Tag = 'linked', [int]$Boot = 80)
. "$PSScriptRoot\lib.ps1"
Assert-Pads $Cabinets
$logs = "$Tools\logs\linktest"; New-Item -ItemType Directory -Force $logs | Out-Null
$env:LL_LOGFILE = "$logs\$Tag-%p.log"

Get-Process Lindy, linuxloader -ErrorAction SilentlyContinue | Stop-Process -Force
$lindy = Start-Process $Lindy -PassThru
Start-Sleep 5
$main = $A::RootElement.FindFirst($S::Children, (Cond $A::ProcessIdProperty $lindy.Id))
$main.FindFirst($S::Descendants, (Cond $A::NameProperty 'SEGA Race TV')).GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select()
Start-Sleep 1
$menu = $main.FindFirst($S::Descendants, (Both (Cond $A::NameProperty 'Game') (Cond $A::ControlTypeProperty $CT::MenuItem)))
$menu.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand(); Start-Sleep 1
$link = $null
foreach ($w in Windows $lindy.Id) { $f = $w.FindFirst($S::Descendants, (Cond $A::NameProperty 'Start Linked Play...')); if ($f) { $link = $f } }
Invoke $link; Start-Sleep 2
# The dialog can be a window of its own or a child of the main window.
$dlg = $null
for ($t = 0; $t -lt 10 -and -not $dlg; $t++) {
    foreach ($w in Windows $lindy.Id) { if ($w.Current.Name -eq 'Linked Play') { $dlg = $w } }
    if (-not $dlg) { $dlg = $main.FindFirst($S::Children, (Cond $A::NameProperty 'Linked Play')) }
    if (-not $dlg) { Start-Sleep 1 }
}
if (-not $dlg) { throw 'The Linked Play dialog did not open.' }
$spin = $dlg.FindFirst($S::Descendants, (Cond $A::ControlTypeProperty $CT::Spinner))
if (-not $spin) { $spin = $dlg.FindFirst($S::Descendants, (Cond $A::ControlTypeProperty $CT::Edit)) }
try { $spin.GetCurrentPattern([System.Windows.Automation.RangeValuePattern]::Pattern).SetValue($Cabinets) }
catch { $spin.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).SetValue("$Cabinets") }
Start-Sleep 1
Invoke ($dlg.FindFirst($S::Descendants, (Both (Cond $A::NameProperty 'Start') (Cond $A::ControlTypeProperty $CT::Button))))
Remove-Item Env:LL_LOGFILE

Start-Sleep $Boot
"cabinets running: $((Cabinets).Count) of $Cabinets"
SnapAll "$Tag-attract"
# Coin, then hold the stick left from Start on, so Multi-Player is already picked when Select Mode
# appears (its 10-second timer runs out if the pads are slow to reach it).
Each 'tap BACK' $Cabinets; Pad @('sleep 1'); Each 'tap BACK' $Cabinets; Pad @('sleep 1')
Each 'stick -1 0' $Cabinets
foreach ($k in 1..3) { Each 'tap START' $Cabinets; Pad @('sleep 2') }
SnapAll "$Tag-mode"
Each 'trigger r 1' $Cabinets; Pad @('sleep 0.4'); Each 'trigger r 0' $Cabinets; Pad @('sleep 1')
Each 'stick 0 0' $Cabinets; Pad @('sleep 10')
SnapAll "$Tag-match"
# Car, transmission and course: gas confirms each.
foreach ($k in 1..5) { Each 'trigger r 1' $Cabinets; Pad @('sleep 0.4'); Each 'trigger r 0' $Cabinets; Pad @('sleep 2.5') }
SnapAll "$Tag-select"
Pad @('sleep 10'); Each 'trigger r 1' $Cabinets; Pad @('sleep 10')
SnapAll "$Tag-race"
Pad @('sleep 8')
SnapAll "$Tag-race2"
Each 'trigger r 0' $Cabinets
"cabinets still running: $((Cabinets).Count)"
foreach ($f in Get-ChildItem $logs -Filter "$Tag-*.log") {
    $crash = Select-String -Path $f.FullName -Pattern 'CRASH' | Select-Object -First 1
    if ($crash) { "$($f.Name): $($crash.Line)" }
}
"screenshots: $Shots\$Tag-*.png"
