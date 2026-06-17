# Consolidated Test Runner for UE-Antigravity Dual-MCP
# This script runs both the C++ headless Unreal Editor automation tests
# and the Python integration/unit tests sequentially.

$ErrorActionPreference = "Stop"

$unreal_exe = "D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$project_path = "c:\Users\Jan\Documents\Unreal Projects\tau-game\Tau.uproject"
$python_exe = "C:\Users\Jan\AppData\Local\Microsoft\WindowsApps\python.exe"

Write-Host "=================================================================" -ForegroundColor Magenta
Write-Host "             UE-Antigravity Consolidated Test Runner            " -ForegroundColor Magenta
Write-Host "=================================================================" -ForegroundColor Magenta

# 1. Verify environment paths
if (-not (Test-Path $unreal_exe)) {
    Write-Error "Unreal Editor command line executable not found at: $unreal_exe"
    exit 1
}
if (-not (Test-Path $project_path)) {
    Write-Error "Target game project not found at: $project_path"
    exit 1
}
if (-not (Test-Path $python_exe)) {
    Write-Error "Windows Store Python not found at: $python_exe"
    exit 1
}

# 2. Run C++ Headless Automation Tests
Write-Host "`n[1/2] Running C++ Headless Unreal Editor Automation Tests..." -ForegroundColor Cyan
Write-Host "Command: UnrealEditor-Cmd.exe -NullRHI -NoSound -NoSourceControl -unattended -ExecCmds=`"Automation RunTests Antigravity; Quit`"" -ForegroundColor DarkGray

# We temporarily override the environment to prevent buffering if possible
$processInfo = New-Object System.Diagnostics.ProcessStartInfo
$processInfo.FileName = $unreal_exe
$processInfo.Arguments = "`"$project_path`" -NullRHI -NoSound -NoSourceControl -unattended -ExecCmds=`"Automation RunTests Antigravity; Quit`" -log"
$processInfo.UseShellExecute = $false
$processInfo.RedirectStandardOutput = $false
$processInfo.RedirectStandardError = $false

$process = [System.Diagnostics.Process]::Start($processInfo)
$process.WaitForExit()
$cpp_exit_code = $process.ExitCode

if ($cpp_exit_code -ne 0) {
    Write-Host "`n[-] C++ Automation Tests FAILED with exit code: $cpp_exit_code" -ForegroundColor Red
    Write-Host "Check the project logs at: c:\Users\Jan\Documents\Unreal Projects\tau-game\Saved\Logs\Tau.log" -ForegroundColor Yellow
    exit $cpp_exit_code
} else {
    Write-Host "`n[+] C++ Headless Automation Tests PASSED successfully!" -ForegroundColor Green
}

# 3. Run Python Unit & Integration Tests
Write-Host "`n[2/2] Running Python unit and integration tests..." -ForegroundColor Cyan
Write-Host "Command: python -m pytest" -ForegroundColor DarkGray

# Trigger python tests
$python_process = Start-Process -FilePath $python_exe -ArgumentList "`"$PSScriptRoot\run_tests.py`"" -Wait -NoNewWindow -PassThru
$python_exit_code = $python_process.ExitCode

if ($python_exit_code -ne 0) {
    Write-Host "`n[-] Python Tests FAILED with exit code: $python_exit_code" -ForegroundColor Red
    exit $python_exit_code
} else {
    Write-Host "`n[+] Python Tests PASSED successfully!" -ForegroundColor Green
}

Write-Host "`n=================================================================" -ForegroundColor Magenta
Write-Host "   [SUCCESS] All C++ and Python tests passed headlessly!   " -ForegroundColor Green
Write-Host "=================================================================" -ForegroundColor Magenta
