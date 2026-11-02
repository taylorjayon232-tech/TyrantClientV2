@echo off
rem Build + run the potion-icon rule harness (pure rules, no JVM and no GL).
rem The header includes imgui.h for ImTextureID/ImU32, so the vendored tree is on the
rem include path; nothing from ImGui is linked or called.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 ( echo vcvars64 failed & exit /b 1 )
cl /nologo /W4 /EHsc /std:c++20 /I"%~dp0..\vendor\imgui" /Fe:"%~dp0item_icons_check.exe" "%~dp0item_icons_check.cpp" || exit /b 1
del /q "%~dp0item_icons_check.obj" >nul 2>&1
"%~dp0item_icons_check.exe"
set RC=%errorlevel%
exit /b %RC%
