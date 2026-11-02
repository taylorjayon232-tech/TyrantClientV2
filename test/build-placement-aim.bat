@echo off
rem Build + run the placement aim (Camera Sensitivity) rules harness (pure rules, no JVM).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 ( echo vcvars64 failed & exit /b 1 )
rem The agent builds with C++20 and this rule header uses std::clamp, so the harness does too.
cl /nologo /W4 /EHsc /std:c++20 /Fe:"%~dp0placement_aim_check.exe" "%~dp0placement_aim_check.cpp" || exit /b 1
"%~dp0placement_aim_check.exe"
exit /b %errorlevel%
