rem ============================================================================
rem  Passive -- fabric\build-jar.bat
rem
rem  Builds the drop-in Fabric mod jar in three steps:
rem    1) the native client (..\build.bat) -- passive_agent.dll + passive_bridge.dll
rem    2) the loader classes, compiled against fabric-loader
rem    3) the jar itself: loader classes + fabric.mod.json + the DLLs + the Inter
rem       faces the interface draws with
rem
rem  Output: fabric\dist\passive-fabric-<version>.jar  -> drop into .minecraft\mods
rem
rem  Requirements: the same Visual Studio 2022 + CMake the native build needs, and
rem  a JDK 21 (JAVA_HOME). fabric-loader is fetched once into fabric\lib\ for the
rem  compile only; it is not bundled into the jar (the loader is already running
rem  the mod when the jar is used).
rem ============================================================================
@echo off
setlocal
cd /d "%~dp0"

set "JAR_VERSION=1.0.2"
set "LOADER_VERSION=0.19.5"
set "LOADER_JAR=lib\fabric-loader-%LOADER_VERSION%.jar"
set "WORK=work"
set "DIST=dist"
set "OUT=%DIST%\passive-fabric-%JAR_VERSION%.jar"

rem ---- 1) native client ------------------------------------------------------
echo [jar] building the native client (passive_agent.dll + passive_bridge.dll) ...
call "%~dp0..\build.bat" || exit /b 1
for %%f in (passive_agent.dll passive_bridge.dll) do (
  if not exist "..\build\Release\%%f" (
    echo [jar] ERROR: ..\build\Release\%%f was not produced by the native build.
    exit /b 1
  )
)

rem ---- 2) JDK ----------------------------------------------------------------
set "JAVAC_EXE=javac"
set "JAR_EXE=jar"
if defined JAVA_HOME (
  if exist "%JAVA_HOME%\bin\javac.exe" set "JAVAC_EXE=%JAVA_HOME%\bin\javac.exe"
  if exist "%JAVA_HOME%\bin\jar.exe"   set "JAR_EXE=%JAVA_HOME%\bin\jar.exe"
)
"%JAVAC_EXE%" -version >nul 2>nul
if errorlevel 1 (
  echo [jar] ERROR: no javac found. Install a JDK 21 and/or set JAVA_HOME.
  exit /b 1
)

rem ---- 3) fabric-loader (compile-time only) ----------------------------------
if not exist "%LOADER_JAR%" (
  if not exist "lib" mkdir "lib"
  echo [jar] fetching fabric-loader %LOADER_VERSION% for the compile ...
  curl -sSL -o "%LOADER_JAR%" "https://maven.fabricmc.net/net/fabricmc/fabric-loader/%LOADER_VERSION%/fabric-loader-%LOADER_VERSION%.jar"
  if errorlevel 1 (
    echo [jar] ERROR: could not fetch fabric-loader. Put the jar at %LOADER_JAR% by hand.
    exit /b 1
  )
)

rem ---- 4) payload ------------------------------------------------------------
echo [jar] staging the payload ...
if exist "%WORK%" rd /s /q "%WORK%"
mkdir "%WORK%\classes"                    || exit /b 1
mkdir "%WORK%\payload\passive\assets\fonts" || exit /b 1

copy /y "..\build\Release\passive_agent.dll"  "%WORK%\payload\passive\passive_agent.dll"  >nul || exit /b 1
copy /y "..\build\Release\passive_bridge.dll" "%WORK%\payload\passive\passive_bridge.dll" >nul || exit /b 1
copy /y "..\assets\passive-logo.png"         "%WORK%\payload\passive\passive-logo.png"  >nul || exit /b 1
copy /y "resources\fabric.mod.json"          "%WORK%\payload\fabric.mod.json"          >nul || exit /b 1
for %%f in (Inter-Regular.ttf Inter-Medium.ttf Inter-SemiBold.ttf) do (
  copy /y "..\assets\fonts\%%f" "%WORK%\payload\passive\assets\fonts\%%f" >nul || exit /b 1
)

rem ---- 5) loader classes -----------------------------------------------------
echo [jar] compiling the loader ...
"%JAVAC_EXE%" -encoding UTF-8 --release 21 -Xlint:all -Werror -cp "%LOADER_JAR%" -d "%WORK%\classes" src\com\passive\fabric\*.java src\com\passive\fabric\modules\*.java || exit /b 1

rem ---- 6) the jar ------------------------------------------------------------
echo [jar] packing %OUT% ...
if not exist "%DIST%" mkdir "%DIST%"
if exist "%OUT%" del "%OUT%"
"%JAR_EXE%" --create --file "%OUT%" -C "%WORK%\classes" . -C "%WORK%\payload" . || exit /b 1

echo.
echo [jar] done: %~dp0%OUT%
echo   contents:
"%JAR_EXE%" --list --file "%OUT%"
echo.
echo Drop it into the instance's mods folder and start the game; RSHIFT opens the
echo menu. Ctrl+Shift+End uninjects (the injector can inject the same DLL after).
exit /b 0
