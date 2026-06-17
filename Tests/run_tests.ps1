# Wrapper script to execute the pytest suite using the correct Windows Store Python interpreter.
# This prevents path shadowing conflicts with MSYS2/MinGW python.

$python_exe = "C:\Users\Jan\AppData\Local\Microsoft\WindowsApps\python.exe"

if (-not (Test-Path $python_exe)) {
    Write-Error "Windows Store Python not found at: $python_exe"
    exit 1
}

Write-Host "Running pytest suite via explicit Python 3.13 interpreter..." -ForegroundColor Cyan
& $python_exe "$PSScriptRoot\run_tests.py"
