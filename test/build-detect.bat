@echo off
rem Build + run the player-detection rules harness (pure rules, no JVM).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 ( echo vcvars64 failed & exit /b 1 )
cl /nologo /W4 /EHsc /Fe:"%~dp0detect_check.exe" "%~dp0detect_check.cpp" || exit /b 1
"%~dp0detect_check.exe"
exit /b %errorlevel%
