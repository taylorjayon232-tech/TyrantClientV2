rem ============================================================================
rem  Tyrant -- vendor/fetch-jdk-headers.bat
rem  Extracts just the JNI headers (jni.h, jni_md.h, classfile_constants.h) from
rem  a JDK 21 distribution. Keeps the repo light and the build reproducible.
rem ============================================================================
@echo off
setlocal
cd /d "%~dp0\.."

if exist thirdparty\jdk-include\jni.h (
  echo [jdk] headers already present.
  exit /b 0
)

set "JDK_URL=https://download.java.net/java/GA/jdk21.0.2/f2283984656d49d69e91c558476027ac/13/GPL/openjdk-21.0.2_windows-x64_bin.zip"

echo [jdk] downloading JDK 21 headers ...
powershell -NoProfile -Command "try { Invoke-WebRequest -Uri $env:JDK_URL -OutFile $env:TEMP\jdk21.zip -UseBasicParsing } catch { exit 1 }" || goto :fail
powershell -NoProfile -Command "try { Expand-Archive -Path $env:TEMP\jdk21.zip -DestinationPath $env:TEMP\jdk21 -Force } catch { exit 1 }" || goto :fail

mkdir thirdparty\jdk-include\win32 2>nul
rem jni.h lives in <jdk>\include, its siblings jni_md.h/classfile_constants.h in
rem the same dir; jni_md.h also exists under <jdk>\include\win32. Copy all.
powershell -NoProfile -Command "try { $src = Get-ChildItem -Path $env:TEMP\jdk21 -Recurse -Filter 'jni.h' | Select-Object -First 1; $inc = $src.Directory.FullName; Copy-Item $src.FullName thirdparty\jdk-include\ ; Copy-Item (Join-Path $inc 'classfile_constants.h') thirdparty\jdk-include\ ; Copy-Item (Join-Path $inc 'win32\jni_md.h') thirdparty\jdk-include\win32\ } catch { exit 1 }" || goto :fail

del %TEMP%\jdk21.zip 2>nul
echo [jdk] headers extracted.
exit /b 0

:fail
echo [jdk] ERROR: could not fetch JDK headers. Install any JDK 17+ and copy
echo        include\jni.h, include\win32\jni_md.h and include\classfile_constants.h
echo        into thirdparty\jdk-include\ manually.
exit /b 1
