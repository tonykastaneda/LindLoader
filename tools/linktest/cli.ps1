# Lindy's command line for frontends: each case starts Lindy.exe with its arguments, reads and closes
# any message box, notes the game it started (then closes it), and checks the exit code.
# Uses the first library folder's RACETV and VT3 game folders. -Only picks cases by name.
param([string[]]$Only)
. "$PSScriptRoot\lib.ps1"
$Only = @($Only | ForEach-Object { $_ -split ',' })
$lib = LibraryFolder
$cases = @(
    @{ Name = 'game-folder';  Args = "`"$lib\RACETV`"";                 Expect = 'starts' }
    @{ Name = 'disk0';        Args = "`"$lib\RACETV\disk0`"";           Expect = 'starts' }
    @{ Name = 'missing';      Args = "`"$lib\NOPE`"";                   Expect = 'Not found' }
    @{ Name = 'library-root'; Args = "`"$lib`"";                        Expect = 'games\. Give the folder' }
    @{ Name = 'not-a-game';   Args = "`"$env:WINDIR\Web`"";             Expect = 'No Lindbergh game' }
    @{ Name = 'no-link-game'; Args = "--linked 2 `"$lib\VT3`"";         Expect = 'has no linked play' }
    @{ Name = 'too-many';     Args = "--linked 5 `"$lib\RACETV`"";      Expect = 'links up to 4' }
    @{ Name = 'linked-1';     Args = "--linked 1 `"$lib\RACETV`"";      Expect = '2 or more' }
    @{ Name = 'linked-word';  Args = "--linked four `"$lib\RACETV`"";   Expect = '2 or more' }
)
# A disk image is refused: any .bin next to the library will do, if there is one.
$bin = Get-ChildItem $lib -Filter *.bin -File -ErrorAction SilentlyContinue | Select-Object -First 1
if ($bin) { $cases += @{ Name = 'disk-image'; Args = "`"$($bin.FullName)`""; Expect = 'needs to be unpacked' } }

$failed = 0
foreach ($c in $cases) {
    if ($Only -and $c.Name -notin $Only) { continue }
    Get-Process linuxloader -ErrorAction SilentlyContinue | Stop-Process -Force
    $p = Start-Process $Lindy -ArgumentList $c.Args -PassThru
    $msg = $null; $started = $null
    for ($t = 0; $t -lt 240 -and -not $p.HasExited; $t++) {
        Start-Sleep -Milliseconds 500
        $l = Get-CimInstance Win32_Process -Filter "name='linuxloader.exe'" | Where-Object { $_.CommandLine -notmatch '--identify' }
        if ($l) { $started = @($l)[0].CommandLine; break }
        $box = $A::RootElement.FindFirst($S::Children, (Cond $A::ProcessIdProperty $p.Id))
        # A box can be found before its text is filled in.
        if ($box) { $msg = Texts $box; if ($msg) { PressButton $box; break } }
    }
    if ($started) { Start-Sleep 2; Get-Process linuxloader -ErrorAction SilentlyContinue | Stop-Process -Force }
    $p.WaitForExit(10000) | Out-Null
    $code = if ($p.HasExited) { $p.ExitCode } else { Stop-Process $p -Force; 'hung' }
    $pass = if ($c.Expect -eq 'starts') { $started -and $code -eq 0 } else { $msg -match $c.Expect -and $code -eq 1 }
    if (-not $pass) { $failed++ }
    "$(if ($pass) { 'PASS' } else { 'FAIL' })  $($c.Name)  [$($c.Args)]  exit $code"
    if ($msg) { "      message: $($msg -replace '\s+', ' ')" }
    if ($started) { "      started: $started" }
}
"$failed failed"
