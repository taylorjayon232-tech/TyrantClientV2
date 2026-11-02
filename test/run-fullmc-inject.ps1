# Tyrant test harness: launch FullMc dummy JVM (real MC classes) + inject.
$ErrorActionPreference = 'Continue'
$test = 'C:\Users\Jayon\Downloads\TyrantClientV2\test'
$mc   = 'C:\Users\Jayon\AppData\Roaming\PrismLauncher\instances\1.21.11\minecraft\.fabric\remappedJars\minecraft-1.21.11-0.19.5\client-intermediary.jar'

# Kill leftovers from previous runs.
Get-Process java -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

$libs = (Get-ChildItem 'C:\Users\Jayon\AppData\Roaming\PrismLauncher\libraries' -Recurse -Filter '*.jar' |
         Where-Object { $_.Name -notmatch 'arm64|x86' } |
         ForEach-Object FullName) -join ';'
$cp = "$test;$mc;$libs"

Remove-Item "$test\fullmc.out" -ErrorAction SilentlyContinue
Remove-Item 'C:\Users\Jayon\AppData\Roaming\PrismLauncher\java\java-runtime-delta\bin\tyrant_agent.log' -ErrorAction SilentlyContinue

$p = Start-Process -FilePath 'C:\Users\Jayon\AppData\Roaming\PrismLauncher\java\java-runtime-delta\bin\java.exe' `
    -ArgumentList '-cp', $cp, 'FullMc' -WorkingDirectory $test `
    -RedirectStandardOutput "$test\fullmc.out" -RedirectStandardError "$test\fullmc.err" -PassThru
Write-Output "jvm pid=$($p.Id)"
Start-Sleep -Seconds 9

& 'C:\Users\Jayon\Downloads\TyrantClientV2\build\Release\tyrant_injector.exe' --pid $p.Id
Write-Output "injector exit=$LASTEXITCODE"
Start-Sleep -Seconds 30   # let fallback path (8s) + full resolve finish

if (Get-Process -Id $p.Id -ErrorAction SilentlyContinue) {
    Write-Output "HOST-SURVIVED pid=$($p.Id)"
    Stop-Process -Id $p.Id -Force
} else {
    Write-Output "HOST-DIED pid=$($p.Id)"
}
