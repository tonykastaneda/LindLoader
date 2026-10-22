' Test launcher: run-logged.vbs <VT3|RACETV|VF5FS>
' No console window; game output to tools\logs\<game>\run.log, the loader's own log to tools\logs\<game>\loader.log.
Set sh = CreateObject("WScript.Shell")
root = "C:\Users\tonyk\Documents\GitHub\Lindy\"
build = WScript.Arguments(0)
base = root & "games\" & build & "\"
logs = root & "tools\logs\" & build & "\"
If build = "VT3" Then gamedir = base & "game\disk0\vt3_Lindbergh" Else gamedir = base & "game\disk0"
sh.Environment("Process")("LL_LOGLEVEL") = "info"
' Read gamepads while the game window is not focused (tests drive virtual pads).
sh.Environment("Process")("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS") = "1"
sh.Environment("Process")("LL_LOGFILE") = logs & "loader.log"
' Extra test variables: tools\testenv.txt, one NAME=VALUE per line.
Set fso = CreateObject("Scripting.FileSystemObject")
envFile = root & "tools\testenv.txt"
If fso.FileExists(envFile) Then
    Set f = fso.OpenTextFile(envFile)
    Do Until f.AtEndOfStream
        kv = Split(Trim(f.ReadLine()), "=", 2)
        If UBound(kv) = 1 Then sh.Environment("Process")(kv(0)) = kv(1)
    Loop
    f.Close
End If
sh.CurrentDirectory = base & "loader"
sh.Run "conhost.exe --headless cmd /c """"" & base & "loader\linuxloader.exe"" -g """ & gamedir & """ -c """ & base & "linuxloader.ini"" -o """ & base & "controls.ini"" > """ & logs & "run.log"" 2>&1""", 1, False
