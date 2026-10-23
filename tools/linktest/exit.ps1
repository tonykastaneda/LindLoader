# Linked play from the command line (Lindy.exe --linked <n> "<game>"), then Exit Game (Start + Back)
# on one cabinet's pad: every cabinet and Lindy must close. Needs vpadd.py with -Cabinets pads.
param([int]$Cabinets = 4, [int]$ExitOn = 2, [int]$Boot = 60)
. "$PSScriptRoot\lib.ps1"
Assert-Pads $Cabinets
$game = "$(LibraryFolder)\RACETV"
Get-Process Lindy, linuxloader -ErrorAction SilentlyContinue | Stop-Process -Force
$p = Start-Process $Lindy -ArgumentList "--linked $Cabinets `"$game`"" -PassThru
Start-Sleep $Boot
"cabinets running: $((Cabinets).Count) of $Cabinets"
Pad @("$ExitOn press START", "$ExitOn press BACK", 'sleep 0.5', "$ExitOn release BACK", "$ExitOn release START")
for ($t = 1; $t -le 10 -and (Cabinets).Count -gt 0; $t++) { Start-Sleep 1 }
"cabinets after Exit Game on cabinet $($ExitOn): $((Cabinets).Count)"
$p.WaitForExit(5000) | Out-Null
"Lindy exited: $($p.HasExited) (exit code $($p.ExitCode))"
$ok = (Cabinets).Count -eq 0 -and $p.HasExited -and $p.ExitCode -eq 0
if ($ok) { 'PASS' } else { 'FAIL' }
