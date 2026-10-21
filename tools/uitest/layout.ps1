# Opens a game's Controls page with the Controllers toolbar button and captures the window.
# Usage: layout.ps1 <game title> <name for the capture>
param([string]$Title, [string]$Name)
. "C:\Users\tonyk\Documents\GitHub\LindLoader\tools\uitest\lib.ps1"
$log = "$T\logs\UI\layout-$Name.log"; "" | Set-Content $log
Get-Process LindLoader -ErrorAction SilentlyContinue | Stop-Process -Force
$p = Start-Process "C:\Users\tonyk\Documents\GitHub\LindLoader\LindLoader.exe" -PassThru
Start-Sleep -Seconds 5
$main = $A::RootElement.FindFirst($S::Children, (New-Object System.Windows.Automation.PropertyCondition($A::ProcessIdProperty, $p.Id)))
Pick (ByName $main $Title); Start-Sleep -Milliseconds 500
$btn = $main.FindFirst($S::Descendants, (New-Object System.Windows.Automation.AndCondition(
    (New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, "Controllers")),
    (New-Object System.Windows.Automation.PropertyCondition($A::ControlTypeProperty, $CT::Button)))))
Log "controllers button enabled: $($btn.Current.IsEnabled)"
Press $btn
$dlg = $null
for ($i = 0; $i -lt 40 -and -not $dlg; $i++) {
    $dlg = $main.FindFirst($S::Children, (New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, "$Title Properties")))
    if (-not $dlg) { Start-Sleep -Milliseconds 500 }
}
Log "dialog: $($dlg.Current.Name)"
Start-Sleep -Seconds 1
(New-Object -ComObject WScript.Shell).Run("$T\wincap.exe $T\shots\layout-$Name.bmp $Title Properties", 0, $true) | Out-Null
