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

:: Compile
cd /d "%~dp0"
cl.exe /EHsc /O2 /std:c++17 main.cpp /Fe:..\bridge.exe Winhttp.lib ws2_32.lib

if %errorlevel% neq 0 (
    echo Compilation failed!
    exit /b %errorlevel%
)

echo Bridge compiled successfully.

:: Get absolute path to bridge.exe and escape backslashes for JSON
for %%i in ("%~dp0..\bridge.exe") do set "BRIDGE_PATH=%%~fi"
set "ESCAPED_PATH=%BRIDGE_PATH:\=\\%"

:: Rewrite mcp_config.json with the relative path
powershell -Command "(Get-Content -Raw ..\mcp_config.json) -replace '\"command\":\s*\"[^\"]+\"', '\"command\": \".agents/plugins/UnrealEngine/bridge.exe\"' ^| Set-Content ..\mcp_config.json"
if %errorlevel% equ 0 (
    echo Configured mcp_config.json with relative path.
)

:: Generate dynamic unreal-env skill
powershell -ExecutionPolicy Bypass -File "%~dp0generate_env_skill.ps1"

