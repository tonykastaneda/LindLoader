. "C:\Users\tonyk\Documents\GitHub\LindLoader\toolsItestib.ps1"
$log = "$T\logs\UI\p2.log"; "" | Set-Content $log
$dlg = OpenControls "Virtua Fighter 5 Final Showdown REV B"
# Player 2 button
$p2 = ByName $dlg "Player 2"
Log "player 2 element: $($p2.Current.ControlType.ProgrammaticName) patterns: $(($p2.GetSupportedPatterns() | % { $_.ProgrammaticName }) -join ',')"
try { Press $p2 } catch { $p2.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Toggle() }; Start-Sleep -Seconds 1
Log "player 2 checked: $($p2.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Current.ToggleState)"
$buttons = Buttons (ByName $dlg "Buttons")
Log "player 2 before: $(($buttons | % { $_.Current.Name }) -join ' | ')"
Press $buttons[0]; Start-Sleep -Milliseconds 800; Pad "2 tap X"; Start-Sleep -Seconds 2
$buttons = Buttons (ByName $dlg "Buttons")
Log "player 2 after: $(($buttons | % { $_.Current.Name }) -join ' | ')"
& "$T\snap.ps1" -Out "$T\shots\ui-p2.png"
