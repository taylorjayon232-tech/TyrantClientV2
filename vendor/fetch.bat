rem ============================================================================
rem  Tyrant -- vendor/fetch.bat
rem  Downloads pinned third-party dependencies into vendor\ using PowerShell.
rem  Run once before building. Requires an internet connection.
rem ============================================================================
@echo off
setlocal
cd /d "%~dp0\.."

set "IMGUI_TAG=v1.91.9b"
set "IMGUI_URL=https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.zip"
set "MINHOOK_URL=https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.3.zip"
set "JSON_URL=https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp"
set "INTER_URL=https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip"

echo [fetch] ImGui %IMGUI_TAG% ...
if not exist vendor\imgui (
  powershell -NoProfile -Command "try { Invoke-WebRequest -Uri $env:IMGUI_URL -OutFile $env:TEMP\imgui.zip -UseBasicParsing } catch { exit 1 }" || goto :fail
  powershell -NoProfile -Command "try { Expand-Archive -Path $env:TEMP\imgui.zip -DestinationPath $env:TEMP\imgui -Force } catch { exit 1 }" || goto :fail
  powershell -NoProfile -Command "try { Move-Item -Path (Join-Path $env:TEMP\imgui 'imgui-1.91.9b') -Destination vendor\imgui -Force } catch { exit 1 }" || goto :fail
  del %TEMP%\imgui.zip 2>nul
)

echo [fetch] MinHook v1.3.3 ...
if not exist vendor\minhook (
  powershell -NoProfile -Command "try { Invoke-WebRequest -Uri $env:MINHOOK_URL -OutFile $env:TEMP\minhook.zip -UseBasicParsing } catch { exit 1 }" || goto :fail
  powershell -NoProfile -Command "try { Expand-Archive -Path $env:TEMP\minhook.zip -DestinationPath $env:TEMP\minhook -Force } catch { exit 1 }" || goto :fail
  powershell -NoProfile -Command "try { Move-Item -Path (Join-Path $env:TEMP\minhook 'minhook-1.3.3') -Destination vendor\minhook -Force } catch { exit 1 }" || goto :fail
  del %TEMP%\minhook.zip 2>nul
)

echo [fetch] nlohmann/json v3.11.3 ...
if not exist vendor\json (
  mkdir vendor\json 2>nul
  powershell -NoProfile -Command "try { Invoke-WebRequest -Uri $env:JSON_URL -OutFile vendor\json\json.hpp -UseBasicParsing } catch { exit 1 }" || goto :fail
)

echo [fetch] Inter font v4.1 ...
if not exist assets\fonts\Inter-Regular.ttf (
  powershell -NoProfile -Command "try { Invoke-WebRequest -Uri $env:INTER_URL -OutFile $env:TEMP\inter.zip -UseBasicParsing } catch { exit 1 }" || goto :fail
  powershell -NoProfile -Command "try { Expand-Archive -Path $env:TEMP\inter.zip -DestinationPath $env:TEMP\inter -Force } catch { exit 1 }" || goto :fail
  mkdir assets\fonts 2>nul
  powershell -NoProfile -Command "try { Copy-Item (Get-ChildItem -Path $env:TEMP\inter -Recurse -Filter 'Inter-Regular.ttf' | Select-Object -First 1).FullName assets\fonts\ ; Copy-Item (Get-ChildItem -Path $env:TEMP\inter -Recurse -Filter 'Inter-SemiBold.ttf' | Select-Object -First 1).FullName assets\fonts\ ; Copy-Item (Get-ChildItem -Path $env:TEMP\inter -Recurse -Filter 'Inter-Medium.ttf' | Select-Object -First 1).FullName assets\fonts\ } catch { exit 1 }" || goto :fail
  del %TEMP%\inter.zip 2>nul
)

echo [fetch] all dependencies present.
exit /b 0

:fail
echo [fetch] ERROR: failed to download dependencies. Check your internet connection.
exit /b 1
