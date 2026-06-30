$DefaultProjectDir = "..\..\.."
$ProjectRoot = Read-Host "Enter the target project root directory [$DefaultProjectDir]"
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = $DefaultProjectDir
}

$PluginDir = $PSScriptRoot

# Try to resolve path
$TargetProjectDir = (Resolve-Path -Path "$PluginDir\$ProjectRoot" -ErrorAction SilentlyContinue).Path
if (-not $TargetProjectDir) {
    $TargetProjectDir = (Resolve-Path -Path $ProjectRoot -ErrorAction SilentlyContinue).Path
}
if (-not $TargetProjectDir) {
    $TargetProjectDir = $ProjectRoot
}

Write-Host "Installing to: $TargetProjectDir"

$AntigravityPluginDir = "$TargetProjectDir\.agents\plugins\UnrealEngine"
$BridgePath = "$PluginDir\bridge.exe"

# 1. Compile Bridge if missing
if (-not (Test-Path $BridgePath)) {
    Write-Host "Compiling MCP Bridge..."
    Start-Process -FilePath "$PluginDir\src\build_bridge.bat" -WorkingDirectory "$PluginDir\src" -Wait -NoNewWindow
}

# 2. Setup Antigravity plugin directory
Write-Host "Setting up Antigravity..."
if (-not (Test-Path $AntigravityPluginDir)) {
    New-Item -ItemType Directory -Force -Path $AntigravityPluginDir | Out-Null
}
$ResolvedPluginDir = (Resolve-Path $PluginDir).Path
$ResolvedDestDir = (Resolve-Path $AntigravityPluginDir).Path
if ($ResolvedPluginDir -ne $ResolvedDestDir) {
    Copy-Item -Path "$PluginDir\*" -Destination $AntigravityPluginDir -Recurse -Force -Exclude "main.obj", "bridge.exe"
    Copy-Item -Path "$PluginDir\bridge.exe" -Destination $AntigravityPluginDir -Force -ErrorAction SilentlyContinue
}

# 2.5 Setup Workspace AGENTS.md
Write-Host "Setting up AGENTS.md at workspace root..."
$TargetAgentsPath = "$TargetProjectDir\AGENTS.md"
$SourceAgentsPath = "$PluginDir\AGENTS.md"
if (-not (Test-Path $SourceAgentsPath)) {
    $SourceAgentsPath = "$AntigravityPluginDir\AGENTS.md"
}

if (Test-Path $SourceAgentsPath) {
    if (-not (Test-Path $TargetAgentsPath)) {
        Copy-Item -Path $SourceAgentsPath -Destination $TargetAgentsPath -Force
        Write-Host "Created AGENTS.md at workspace root."
    } else {
        $existingContent = Get-Content -Path $TargetAgentsPath -Raw
        if ($existingContent -notmatch "Agent Efficiency & Robustness Guidelines") {
            Add-Content -Path $TargetAgentsPath -Value "`n`n$(Get-Content -Path $SourceAgentsPath -Raw)"
            Write-Host "Appended Agent Efficiency & Robustness Guidelines to existing AGENTS.md."
        } else {
            Write-Host "AGENTS.md at workspace root already contains the efficiency guidelines."
        }
    }
}

# 3. Select AI coding assistant
Write-Host ""
Write-Host "Which AI coding assistant do you want to configure?"
Write-Host "  [0] Antigravity 2.0 (default)"
Write-Host "  [1] Kilo Code"
$clientChoice = Read-Host "Select (press Enter for Antigravity 2.0)"
$useKiloCode = ($clientChoice -eq "1")

# Resolve dynamic python path
$UserDir = $env:USERPROFILE
$StorePython = "$UserDir\AppData\Local\Microsoft\WindowsApps\python.exe"
if (Test-Path $StorePython) {
    $PythonExe = $StorePython
} else {
    $PythonExe = (Get-Command python -ErrorAction SilentlyContinue).Source
    if (-not $PythonExe) { $PythonExe = "python" }
}

# Setup dedicated virtual environment for External Server in destination
Write-Host "Setting up Python virtual environment..."
$VenvDir = "$AntigravityPluginDir\ExternalServer\.venv"
if (-not (Test-Path $VenvDir)) {
    & $PythonExe -m venv $VenvDir
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to create Python virtual environment."
        exit 1
    }
}
$VenvPython = "$VenvDir\Scripts\python.exe"
$PythonExeEscaped = $VenvPython -replace '\\', '\\'

# Install External Server runtime dependencies via pip
Write-Host "Installing External Server dependencies via pip..."
& $VenvPython -m pip install -r "$AntigravityPluginDir\ExternalServer\requirements.txt"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to install Python dependencies via pip. Setup process failed."
    exit 1
}

# Resolve repository root dynamically (parent of the UnrealEngine folder)
$RepoRoot = (Get-Item $PluginDir).Parent.FullName -replace '\\', '\\'
$PluginDirEscaped = $AntigravityPluginDir -replace '\\', '\\'
$BridgeDestPath = "$AntigravityPluginDir\bridge.exe" -replace '\\', '\\'

if ($useKiloCode) {
    # ── Kilo Code ────────────────────────────────────────────────────────────

    # Ask for LLM profile
    Write-Host ""
    Write-Host "Available LLM profiles for Kilo Code:"
    $profiles = Get-ChildItem -Path "$PluginDir\profiles\*.json" | ForEach-Object { $_.BaseName }
    for ($i = 0; $i -lt $profiles.Count; $i++) {
        Write-Host "  [$i] $($profiles[$i])"
    }
    $profileChoice = Read-Host "Select a profile (enter number, or press Enter for 'default')"
    $selectedProfile = "default"
    if (-not [string]::IsNullOrWhiteSpace($profileChoice)) {
        $idx = [int]$profileChoice
        if ($idx -ge 0 -and $idx -lt $profiles.Count) {
            $selectedProfile = $profiles[$idx]
        }
    }
    Write-Host "Using profile: $selectedProfile"

    # Setup Kilo Code rules (skill hard-links)
    $KiloCodeRulesDir = "$TargetProjectDir\.kilocode\rules"
    Write-Host "Setting up Kilo Code rules..."
    if (-not (Test-Path $KiloCodeRulesDir)) {
        New-Item -ItemType Directory -Force -Path $KiloCodeRulesDir | Out-Null
    }

    $SkillsSourceDir = "$AntigravityPluginDir\skills"
    if (-not (Test-Path $SkillsSourceDir)) { $SkillsSourceDir = "$PluginDir\skills" }

    $LastTargetRule = ""
    Get-ChildItem -Path $SkillsSourceDir -Directory | ForEach-Object {
        $SkillName = $_.Name
        $SourceSkill = Join-Path $_.FullName "SKILL.md"
        $LastTargetRule = Join-Path $KiloCodeRulesDir "$SkillName.md"
        if (Test-Path $LastTargetRule) { Remove-Item $LastTargetRule }
        if (Test-Path $SourceSkill) {
            New-Item -ItemType HardLink -Path $LastTargetRule -Value $SourceSkill | Out-Null
            Write-Host "  Linked skill '$SkillName'."
        }
    }

    # Write kilo.jsonc
    $KiloConfigPath = "$TargetProjectDir\kilo.jsonc"
    $KiloConfigContent = @"
{
  "mcpServers": {
    "unrealengine": {
      "type": "stdio",
      "command": "$BridgeDestPath",
      "env": {
        "BRIDGE_PROFILE": "$selectedProfile"
      },
      "enabled": true
    },
    "cpp-ast-rag": {
      "type": "stdio",
      "command": "$PythonExeEscaped",
      "args": ["-u", "-m", "ExternalServer.src.main"],
      "env": {
        "PYTHONPATH": "$PluginDirEscaped"
      },
      "enabled": true
    }
  }
}
"@

    if (-not (Test-Path $KiloConfigPath)) {
        Set-Content -Path $KiloConfigPath -Value $KiloConfigContent
        Write-Host "Created kilo.jsonc."
    } else {
        Write-Host "kilo.jsonc already exists. Please manually merge the MCP server definition:"
        Write-Host $KiloConfigContent
    }

    Write-Host ""
    Write-Host "Installation Complete (Kilo Code)."
    Write-Host "  Antigravity plugin : $AntigravityPluginDir"
    Write-Host "  Kilo Code rules    : $KiloCodeRulesDir"
    Write-Host "  LLM Profile        : $selectedProfile"

} else {
    # ── Antigravity 2.0 (default) ────────────────────────────────────────────

    $McpConfigContent = @"
{
  "mcpServers": {
    "unrealengine": {
      "type": "stdio",
      "command": "$BridgeDestPath",
      "enabled": true
    },
    "cpp-ast-rag": {
      "type": "stdio",
      "command": "$PythonExeEscaped",
      "args": ["-u", "-m", "ExternalServer.src.main"],
      "env": {
        "PYTHONPATH": "$PluginDirEscaped"
      },
      "enabled": true
    }
  }
}
"@

    $PluginConfigPath = Join-Path $AntigravityPluginDir "mcp_config.json"
    Set-Content -Path $PluginConfigPath -Value $McpConfigContent
    Write-Host "Created/Updated mcp_config.json."

    Write-Host ""
    Write-Host "Installation Complete (Antigravity 2.0)."
    Write-Host "  Antigravity plugin : $AntigravityPluginDir"
    Write-Host "  MCP config         : $PluginConfigPath"
}

Write-Host ""
Write-Host "------------------------------------------------------------------------"
Write-Host "Next Step (Highly Recommended):"
Write-Host "  Ensure Unreal Editor is running, open a new conversation with your"
if ($useKiloCode) {
    Write-Host "  AI assistant (which will follow the linked Kilo rules), and ask to:"
} else {
    Write-Host "  AI assistant, and prompt it to run the setup:"
}
Write-Host ""
Write-Host "    'Run project setup and index the project'"
Write-Host ""
Write-Host "  This will scan your project structures, generate the local 'unreal-env'"
Write-Host "  skill, and build a permanent OKF project-index skill to optimize all"
Write-Host "  subsequent agent conversations."
Write-Host "------------------------------------------------------------------------"

