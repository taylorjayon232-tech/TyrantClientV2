@echo off
rem Build the window-class diagnostic.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 ( echo vcvars64 failed & exit /b 1 )
cl /nologo /W4 /Fe:"%~dp0windowscan.exe" "%~dp0windowscan.c" user32.lib
