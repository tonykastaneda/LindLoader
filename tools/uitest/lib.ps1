# UI Automation helpers for driving LindLoader without keyboard focus.
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class W { [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l); }
"@
$T = "C:\Users\tonyk\Documents\GitHub\LindLoader\tools"
$A = [System.Windows.Automation.AutomationElement]; $S = [System.Windows.Automation.TreeScope]
$CT = [System.Windows.Automation.ControlType]
function Log($m) { Add-Content $log $m }
function ByName($root, $name) {
    for ($i = 0; $i -lt 20; $i++) {
        $e = $root.FindFirst($S::Descendants, (New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, $name)))
        if ($e) { return $e }; Start-Sleep -Milliseconds 300
    }
    Log "not found: $name"; return $null
}
function Pick($e) { $e.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select() }
function Press($e) { $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke() }
function Buttons($group) {
    return $group.FindAll($S::Descendants, (New-Object System.Windows.Automation.PropertyCondition($A::ControlTypeProperty, $CT::Button)))
}
function Pad($cmd) { Add-Content "$T\cmd.txt" $cmd }
# Opens <title>'s Properties on the Controls page and returns the window.
function OpenControls($title) {
    Get-Process LindLoader -ErrorAction SilentlyContinue | Stop-Process -Force
    $p = Start-Process "C:\Users\tonyk\Documents\GitHub\LindLoader\LindLoader.exe" -PassThru
    Start-Sleep -Seconds 5
    $script:main = $A::RootElement.FindFirst($S::Children, (New-Object System.Windows.Automation.PropertyCondition($A::ProcessIdProperty, $p.Id)))
    Log "main: $($main.Current.Name) pid $($p.Id)"
    Pick (ByName $main $title); Start-Sleep -Milliseconds 500
    $props = $main.FindFirst($S::Descendants, (New-Object System.Windows.Automation.AndCondition(
        (New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, "Properties")),
        (New-Object System.Windows.Automation.PropertyCondition($A::ControlTypeProperty, $CT::Button)))))
    Log "properties button: $($props.Current.Name)"
    Log "properties enabled: $($props.Current.IsEnabled)"
    Press $props; Start-Sleep -Seconds 3
    $wins = $A::RootElement.FindAll($S::Children, (New-Object System.Windows.Automation.PropertyCondition($A::ProcessIdProperty, $p.Id)))
    Log "top-level windows: $(($wins | % { $_.Current.Name }) -join ' | ')"
    $sub = $main.FindAll($S::Children, (New-Object System.Windows.Automation.PropertyCondition($A::ControlTypeProperty, $CT::Window)))
    Log "child windows: $(($sub | % { $_.Current.Name }) -join ' | ')"
    $dlg = $null
    for ($i = 0; $i -lt 40 -and -not $dlg; $i++) {
        $dlg = $main.FindFirst($S::Children, (New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, "$title Properties")))
        if (-not $dlg) { Start-Sleep -Milliseconds 500 }
    }
    Log "dialog: $($dlg.Current.Name)"
    Pick (ByName $dlg "Controls"); Start-Sleep -Seconds 1
    return $dlg
}
