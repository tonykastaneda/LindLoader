' Runs snap.ps1 with no window (for the LL_Snap task).
CreateObject("WScript.Shell").Run "powershell -NoProfile -WindowStyle Hidden -File C:\Users\tonyk\Documents\GitHub\LindLoader\tools\snap.ps1 -Out C:\Users\tonyk\Documents\GitHub\LindLoader\tools\latest.png", 0, True
