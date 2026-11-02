@echo off
rem Build + run the translatable item-name rule harness (pure rules, no JVM).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 ( echo vcvars64 failed & exit /b 1 )
cl /nologo /W4 /EHsc /std:c++20 /Fe:"%~dp0text_name_check.exe" "%~dp0text_name_check.cpp" || exit /b 1
"%~dp0text_name_check.exe"
exit /b %errorlevel%
