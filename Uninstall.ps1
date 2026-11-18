$ErrorActionPreference = 'Stop'
$directory = Join-Path $env:LOCALAPPDATA 'TypoPrank'
$target = Join-Path $directory 'TypoPrank.exe'
$run = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
Remove-ItemProperty -Path $run -Name 'TypoPrank' -ErrorAction SilentlyContinue
Get-CimInstance Win32_Process -Filter "Name = 'TypoPrank.exe'" |
    ForEach-Object {
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
        Wait-Process -Id $_.ProcessId -Timeout 5 -ErrorAction SilentlyContinue
    }
if (Get-CimInstance Win32_Process -Filter "Name = 'TypoPrank.exe'") { throw 'Close the running Typo Prank before uninstalling.' }
if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
if (Test-Path -LiteralPath $directory) { Remove-Item -LiteralPath $directory -ErrorAction SilentlyContinue }
Write-Host 'Uninstalled.'
