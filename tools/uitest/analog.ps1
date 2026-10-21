. "C:\Users\tonyk\Documents\GitHub\LindLoader\tools\uitest\lib.ps1"
$log = "$T\logs\UI\analog.log"; "" | Set-Content $log
try { $dlg = OpenControls "SEGA Race TV" } catch { Log "error: $_" }
$all = Buttons (ByName $dlg "Steering & Pedals"); $steering = @($all[0], $all[1]); $pedals = @($all[3], $all[4])
Log "before: steering = $(($steering | % { $_.Current.Name }) -join ' | ')"
Log "before: pedals = $(($pedals | % { $_.Current.Name }) -join ' | ')"
# Steering (analog) <- left stick right
Press $steering[0]; Start-Sleep -Milliseconds 800; Pad "1 stick 1.0 0"; Start-Sleep -Seconds 1; Pad "1 stick 0 0"; Start-Sleep -Seconds 1
# Steer Left (digital) <- left stick left
Press $steering[1]; Start-Sleep -Milliseconds 800; Pad "1 stick -1.0 0"; Start-Sleep -Seconds 1; Pad "1 stick 0 0"; Start-Sleep -Seconds 1
# Gas (analog) <- right trigger
Press $pedals[0]; Start-Sleep -Milliseconds 800; Pad "1 trigger r 1.0"; Start-Sleep -Seconds 1; Pad "1 trigger r 0"; Start-Sleep -Seconds 1
# Gas (analog) must ignore a button press: A, then nothing should change before the timeout
Press $pedals[1]; Start-Sleep -Milliseconds 800; Pad "1 tap A"; Start-Sleep -Seconds 6
$all = Buttons (ByName $dlg "Steering & Pedals"); $steering = @($all[0], $all[1]); $pedals = @($all[3], $all[4])
Log "after: steering = $(($steering | % { $_.Current.Name }) -join ' | ')"
Log "after: pedals = $(($pedals | % { $_.Current.Name }) -join ' | ')"
& "$T\snap.ps1" -Out "$T\shots\ui-analog.png"
