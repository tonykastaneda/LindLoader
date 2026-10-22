. "C:\Users\tonyk\Documents\GitHub\Lindy\tools\uitest\lib.ps1"
$log = "$T\logs\UI\p2.log"; "" | Set-Content $log
$dlg = OpenControls "Virtua Fighter 5 Final Showdown REV B"
$buttons = Buttons (ByName $dlg "Buttons")
Log "player 2 before: $(($buttons | % { $_.Current.Name }) -join ' | ')"
Press $buttons[1]; Start-Sleep -Milliseconds 800; Pad "2 tap X"; Start-Sleep -Seconds 2
$buttons = Buttons (ByName $dlg "Buttons")
Log "player 2 after: $(($buttons | % { $_.Current.Name }) -join ' | ')"
& "$T\snap.ps1" -Out "$T\shots\ui-p2.png"
