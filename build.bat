rem ============================================================================
rem  Passive -- build.bat
rem  One-shot build script: fetches dependencies, configures CMake, builds both
rem  targets. Requires Visual Studio 2022 (with C++ workload) on PATH or its
rem  default install location. Produces build\Release\passive_agent.dll and
rem  build\Release\passive_injector.exe.
rem ============================================================================
@echo off
setlocal
cd /d "%~dp0"

echo [build] fetching dependencies ...
call vendor\fetch.bat           || exit /b 1

rem Locate Visual Studio's CMake via vswhere.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "CMAKE_EXE="
set "VCVARS="

if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VSROOT=%%i"
  )
)
if defined VSROOT (
  if exist "%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE_EXE=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  )
  if exist "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
  )
)

if not defined CMAKE_EXE (
  where cmake >nul 2>nul && set "CMAKE_EXE=cmake"
)
if not defined CMAKE_EXE (
  echo [build] ERROR: no CMake found. Install "C++ CMake tools" in Visual Studio 2022.
  exit /b 1
)

if defined VCVARS (
  call "%VCVARS%" >nul 2>nul
)

echo [build] configuring with %CMAKE_EXE% ...
"%CMAKE_EXE%" -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release || exit /b 1
"%CMAKE_EXE%" --build build --config Release --parallel || exit /b 1

echo.
echo [build] done. Outputs:
echo   build\Release\passive_agent.dll
echo   build\Release\passive_injector.exe
exit /b 0
