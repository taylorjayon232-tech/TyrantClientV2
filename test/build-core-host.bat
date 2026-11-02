@echo off
rem Build the core-profile GL test host (mirrors Minecraft's GL 3.2 core context).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 ( echo vcvars64 failed & exit /b 1 )
cl /nologo /W4 /Fe:"%~dp0wglhost_core.exe" "%~dp0wglhost_core.c" user32.lib gdi32.lib opengl32.lib
