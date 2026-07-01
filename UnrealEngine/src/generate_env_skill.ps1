$PSScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PluginDir = (Resolve-Path (Join-Path $PSScriptDir "..")).Path

# Try to find the uproject file dynamically by searching upwards
$ProjectRoot = $PluginDir
$UprojectFile = $null
for ($i = 0; $i -lt 5; $i++) {
    $Files = Get-ChildItem -Path $ProjectRoot -Filter *.uproject -ErrorAction SilentlyContinue
    if ($Files) {
        $UprojectFile = $Files[0].FullName
        break
    }
    $ProjectRoot = Split-Path -Parent $ProjectRoot
}

if (-not $UprojectFile) {
    # Scan sibling directories (parent of repo root)
    $RepoParent = (Resolve-Path (Join-Path $PluginDir "../..")).Path
    $SiblingUprojects = Get-ChildItem -Path $RepoParent -Filter *.uproject -Recurse -Depth 2 -ErrorAction SilentlyContinue
    if ($SiblingUprojects) {
        $TauProject = $SiblingUprojects | Where-Object { $_.Name -like "*Tau*" -or $_.DirectoryName -like "*tau-game*" }
        if ($TauProject) {
            $UprojectFile = $TauProject[0].FullName
        } else {
            $UprojectFile = $SiblingUprojects[0].FullName
        }
    } else {
        # Default fallback
        $UprojectFile = Join-Path $RepoParent "tau-game\Tau.uproject"
    }
}

$ProjectFile = $UprojectFile
$ProjectName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectFile)
$EditorTarget = $ProjectName + "Editor"

# Parse .uproject to extract EngineAssociation
$EngineAssociation = "5.8" # Default fallback
if (Test-Path $ProjectFile) {
    try {
        $UprojectJson = Get-Content -Raw -Path $ProjectFile | ConvertFrom-Json
        if ($UprojectJson.EngineAssociation) {
            $EngineAssociation = $UprojectJson.EngineAssociation
        }
    } catch {
        Write-Host "Warning: Failed to parse $ProjectFile as JSON. Using fallback EngineAssociation." -ForegroundColor Yellow
    }
}

$InstalledDir = $null

# Resolve InstalledDirectory based on EngineAssociation
if ($EngineAssociation -match "^\d+\.\d+$") {
    # Standard version string (e.g. "5.8")
    $RegistryPath = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$EngineAssociation"
    $InstalledDir = (Get-ItemProperty -Path $RegistryPath -Name InstalledDirectory -ErrorAction SilentlyContinue).InstalledDirectory
    
    # Check HKCU builds in case they registered standard version there
    if (-not $InstalledDir) {
        $BuildsKey = "HKCU:\Software\Epic Games\Unreal Engine\Builds"
        if (Test-Path $BuildsKey) {
            $InstalledDir = (Get-ItemProperty -Path $BuildsKey -Name $EngineAssociation -ErrorAction SilentlyContinue).$EngineAssociation
        }
    }

    # Fallback to standard paths if registry lookup fails
    if (-not $InstalledDir) {
        $FallbackPaths = @(
            "C:\Program Files\Epic Games\UE_$EngineAssociation",
            "C:\Program Files (x86)\Epic Games\UE_$EngineAssociation",
            "D:\Epic Games\UE_$EngineAssociation"
        )
        foreach ($p in $FallbackPaths) {
            if (Test-Path $p) {
                $InstalledDir = $p
                break
            }
        }
    }
} else {
    # Custom or source-built engine (GUID or custom name)
    $BuildsKey = "HKCU:\Software\Epic Games\Unreal Engine\Builds"
    if (Test-Path $BuildsKey) {
        $InstalledDir = (Get-ItemProperty -Path $BuildsKey -Name $EngineAssociation -ErrorAction SilentlyContinue).$EngineAssociation
    }
}

# Ultimate fallback
if (-not $InstalledDir) {
    $InstalledDir = "C:\Program Files\Epic Games\UE_5.8"
}

# Clean trailing slashes
$InstalledDir = $InstalledDir.TrimEnd('\')

$EditorExe = Join-Path $InstalledDir "Engine\Binaries\Win64\UnrealEditor.exe"
$BuildTool = Join-Path $InstalledDir "Engine\Build\BatchFiles\Build.bat"
$DevAuthTool = "C:\Program Files\EOS_DevAuthTool\EOS_DevAuthTool.exe"

$UnrealInstructionsPath = Join-Path $PluginDir "skills\unreal-instructions\SKILL.md"

$LocalEnvContent = @"
<!-- LOCAL_ENV_START -->
## 2. Local Environment & Workflows

### Startup Requirement
When this plugin first loads, you **MUST** call ``ask_permission`` with:
- ``Action``: ``"unsandboxed"``
- ``Target``: ``"$EditorExe"``

This grants the ability to launch the Unreal Editor process outside the terminal sandbox. Do this proactively before any editor interaction is needed.

### Local Paths
- **Unreal Engine Root:** $InstalledDir
- **Unreal Editor Executable:** $EditorExe
- **Unreal Build Tool:** $BuildTool
- **Unreal Project File:** $ProjectFile
- **EOS DevAuthTool:** $DevAuthTool

### Developer Tool Workflows
#### 1. EOS DevAuthTool
Only start this tool when testing multiplayer/online functionality or when explicitly requested. When required:
``````powershell
Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = '"$DevAuthTool"' }
``````
Wait until the user has interacted with the opened EOS_DevAuthTool window (e.g. to log in, accept scopes, or configure credentials) before trying to use it for authentication.

#### 2. Building the Project
To compile the C++ code and binaries for the editor (e.g. when editor is closed, to prevent out-of-date binaries preventing launch):
``````powershell
Start-Process -FilePath "$BuildTool" -ArgumentList "$EditorTarget", "Win64", "Development", '"$ProjectFile"', "-WaitMutex" -Wait -NoNewWindow
``````

#### 3. Launching the Unreal Editor
##### A. Standard Launch (Default / Single-player / Local Testing)
Launch the editor without online/EOS arguments (DevAuthTool is not required):
``````powershell
Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = '"$EditorExe" "$ProjectFile"' }
``````

##### B. Multiplayer / EOS Launch (Only when testing multiplayer or explicitly requested)
Start the EOS DevAuthTool first, then launch the editor with developer authentication arguments:
``````powershell
Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = '"$EditorExe" "$ProjectFile" -CustomConfig=EOS -AUTH_TYPE=developer -AUTH_LOGIN=localhost:8080 -AUTH_PASSWORD=TauDev' }
``````
<!-- LOCAL_ENV_END -->
"@

if (Test-Path $UnrealInstructionsPath) {
    $InstructionsContent = Get-Content -Raw -Path $UnrealInstructionsPath
    $Pattern = "(?s)<!-- LOCAL_ENV_START -->.*?<!-- LOCAL_ENV_END -->"
    
    if ($InstructionsContent -match $Pattern) {
        $NewContent = $InstructionsContent -replace $Pattern, $LocalEnvContent
        Set-Content -Path $UnrealInstructionsPath -Value $NewContent -Encoding UTF8
        Write-Host "Successfully merged dynamic environment into unreal-instructions/SKILL.md"
    } else {
        Write-Error "Could not find LOCAL_ENV markers in unreal-instructions/SKILL.md"
    }
} else {
    Write-Error "Could not find unreal-instructions/SKILL.md at $UnrealInstructionsPath"
}
