@echo off
setlocal

:: Find vcvarsall.bat
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" (
    echo vswhere.exe not found.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "vs_path=%%i"
)

if "%vs_path%"=="" (
    echo Visual Studio with C++ tools not found.
    exit /b 1
)

call "%vs_path%\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Compile to a temporary name first to avoid locking issues
cd /d "%~dp0"
cl.exe /EHsc /O2 /std:c++17 main.cpp /Fe:..\bridge_new.exe Winhttp.lib ws2_32.lib

if %errorlevel% neq 0 (
    echo Compilation failed!
    exit /b %errorlevel%
)

:: Swap the binaries (handles the case where bridge.exe is locked by a running process)
if exist "..\bridge.exe" (
    move /Y "..\bridge.exe" "..\bridge_old.exe" >nul 2>&1
    if %errorlevel% neq 0 (
        echo WARNING: Could not replace running bridge.exe. The new binary is at bridge_new.exe.
        echo Restart Antigravity/Kilo Code and re-run this script, or manually rename bridge_new.exe to bridge.exe.
        exit /b 0
    )
    del "..\bridge_old.exe" >nul 2>&1
)
move /Y "..\bridge_new.exe" "..\bridge.exe" >nul

echo Bridge compiled successfully.

:: Get absolute path to bridge.exe and escape backslashes for JSON
for %%i in ("%~dp0..\bridge.exe") do set "BRIDGE_PATH=%%~fi"
set "ESCAPED_PATH=%BRIDGE_PATH:\=\\%"

:: Rewrite mcp_config.json with the absolute path
powershell -Command "(Get-Content -Raw ..\mcp_config.json) -replace '\"command\":\s*\"[^\"]+\"', '\"command\": \"%ESCAPED_PATH%\"' ^| Set-Content ..\mcp_config.json"
if %errorlevel% equ 0 (
    echo Configured mcp_config.json with absolute path.
)

