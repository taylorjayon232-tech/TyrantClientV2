@echo off
rem Tyrant test helper: build the WGL test host with MSVC env.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
  echo vcvars64 failed
  exit /b 1
)
cl /nologo /Fe:"%~dp0wglhost.exe" "%~dp0wglhost.c" user32.lib gdi32.lib opengl32.lib
