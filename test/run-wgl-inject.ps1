# Tyrant test harness: launch WGL host + inject + collect agent log.
$ErrorActionPreference = 'Continue'
$test = 'C:\Users\Jayon\Downloads\TyrantClientV2\test'
$log  = Join-Path $test 'tyrant_agent.log'   # our own log only - the java-bin log belongs to FullMc

Get-Process wglhost -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item $log -ErrorAction SilentlyContinue

$p = Start-Process -FilePath "$test\wglhost.exe" -WorkingDirectory $test `
    -RedirectStandardOutput "$test\wgl-host.log" -PassThru
Write-Output "host pid=$($p.Id)"
Start-Sleep -Seconds 3

& 'C:\Users\Jayon\Downloads\TyrantClientV2\build\Release\tyrant_injector.exe' --pid $p.Id
Write-Output "injector exit=$LASTEXITCODE"
Start-Sleep -Seconds 15   # plenty of render frames

if (Get-Process -Id $p.Id -ErrorAction SilentlyContinue) { Write-Output "HOST-SURVIVED pid=$($p.Id)" }
else { Write-Output "HOST-DIED pid=$($p.Id)" }
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

Write-Output "=== agent log ==="
if (Test-Path $log) { Get-Content $log } else { Write-Output "(no agent log)" }
