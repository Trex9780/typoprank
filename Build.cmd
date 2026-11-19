@echo off
for /f "usebackq delims=" %%V in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_DIR=%%V"
if not defined VS_DIR exit /b 1
call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE TypoPrank.cpp /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib /OUT:TypoPrank.exe
exit /b %errorlevel%
