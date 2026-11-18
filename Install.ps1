$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'TypoPrank.exe'
if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw 'TypoPrank.exe must be beside this installer.' }
$directory = Join-Path $env:LOCALAPPDATA 'TypoPrank'
$target = Join-Path $directory 'TypoPrank.exe'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
Get-CimInstance Win32_Process -Filter "Name = 'TypoPrank.exe'" |
    ForEach-Object {
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
        Wait-Process -Id $_.ProcessId -Timeout 5 -ErrorAction SilentlyContinue
    }
if (Get-CimInstance Win32_Process -Filter "Name = 'TypoPrank.exe'") { throw 'Close the running Typo Prank before installing.' }
Copy-Item -LiteralPath $source -Destination $target -Force
if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash) { throw 'The installed copy did not match the update.' }
$run = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
New-ItemProperty -Path $run -Name 'TypoPrank' -Value ('"' + $target + '"') -PropertyType String -Force | Out-Null
Start-Process -FilePath $target -WindowStyle Hidden
Write-Host 'Installed or updated. It will start when this Windows user signs in. Right-click the Typo Prank tray icon to pause or exit.'
