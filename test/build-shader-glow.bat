@echo off
rem Build + run the Shader ESP rules harness (pure rules, no JVM, no GL).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 ( echo vcvars64 failed & exit /b 1 )
cl /nologo /W4 /EHsc /Fe:"%~dp0shader_glow_check.exe" "%~dp0shader_glow_check.cpp" || exit /b 1
"%~dp0shader_glow_check.exe"
exit /b %errorlevel%
