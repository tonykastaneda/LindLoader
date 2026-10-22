# Drives LindLoader with UI Automation (no keyboard focus needed): VF5 Properties > Controls,
# binds Button 1 with virtual pad 1 (X) and Button 2 with the K key (posted to LindLoader's window).
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class W { [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l); }
"@
$T = "C:\Users\tonyk\Documents\GitHub\Lindy\tools"
$log = "$T\logs\UI\bind.log"; "" | Set-Content $log
function Log($m) { Add-Content $log $m }
$A = [System.Windows.Automation.AutomationElement]; $S = [System.Windows.Automation.TreeScope]
function Find($root, $name, $type) {
    $c = New-Object System.Windows.Automation.AndCondition(
        (New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, $name)),
        (New-Object System.Windows.Automation.PropertyCondition($A::ControlTypeProperty, $type)))
    for ($i = 0; $i -lt 20; $i++) { $e = $root.FindFirst($S::Descendants, $c); if ($e) { return $e }; Start-Sleep -Milliseconds 300 }
    Log "not found: $name"; return $null
}
$CT = [System.Windows.Automation.ControlType]
Get-Process Lindy -ErrorAction SilentlyContinue | Stop-Process -Force
$p = Start-Process "C:\Users\tonyk\Documents\GitHub\Lindy\Lindy.exe" -PassThru
Start-Sleep -Seconds 5
$main = $A::RootElement.FindFirst($S::Children, (New-Object System.Windows.Automation.PropertyCondition($A::ProcessIdProperty, $p.Id)))
Log "main: $($main.Current.Name)"
$row = $main.FindFirst($S::Descendants, (New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, "Virtua Fighter 5 Final Showdown REV B")))
Log "row: $($row.Current.ControlType.ProgrammaticName)"
$row.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select()
Start-Sleep -Milliseconds 500
$props = Find $main "Properties" $CT::Button
$props.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Start-Sleep -Seconds 3
$dlg = $A::RootElement.FindFirst($S::Children, (New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, "Virtua Fighter 5 Final Showdown REV B Properties")))
if (-not $dlg) { $dlg = Find $main "Virtua Fighter 5 Final Showdown REV B Properties" $CT::Window }
Log "dialog: $($dlg.Current.Name) hwnd $($dlg.Current.NativeWindowHandle)"
$controls = Find $dlg "Controls" $CT::ListItem
$controls.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select()
Start-Sleep -Seconds 1
$all = $dlg.FindAll($S::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
Log "tree ($($all.Count)):"; foreach ($e in $all) { Log ("  {0} | {1} | offscreen={2}" -f $e.Current.ControlType.ProgrammaticName, $e.Current.Name, $e.Current.IsOffscreen) }
$group = $null
for ($i = 0; $i -lt 20 -and -not $group; $i++) {
    $group = $dlg.FindFirst($S::Descendants, (New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, "Buttons")))
    if (-not $group) { Start-Sleep -Milliseconds 300 }
}
Log "group: $($group.Current.ControlType.ProgrammaticName)"
$buttons = $group.FindAll($S::Descendants, (New-Object System.Windows.Automation.PropertyCondition($A::ControlTypeProperty, $CT::Button)))
Log "buttons in group: $($buttons.Count): $(($buttons | % { $_.Current.Name }) -join ' | ')"
# Button 1 <- pad X
$buttons[0].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Start-Sleep -Milliseconds 800
Add-Content "$T\cmd.txt" "1 tap X"; Start-Sleep -Seconds 2
# Button 2 <- K (scancode 0x25), posted to the dialog; the binding button has grabbed the keyboard.
$buttons[2].GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
Start-Sleep -Milliseconds 800
$h = [IntPtr]$dlg.Current.NativeWindowHandle
[W]::PostMessage($h, 0x100, [IntPtr]0x4B, [IntPtr](1 -bor (0x25 -shl 16))) | Out-Null
[W]::PostMessage($h, 0x101, [IntPtr]0x4B, [IntPtr](1 -bor (0x25 -shl 16) -bor (3 -shl 30))) | Out-Null
Start-Sleep -Seconds 2
$buttons = $group.FindAll($S::Descendants, (New-Object System.Windows.Automation.PropertyCondition($A::ControlTypeProperty, $CT::Button)))
Log "after: $(($buttons | % { $_.Current.Name }) -join ' | ')"
& "$T\snap.ps1" -Out "$T\shots\ui-bind.png"
