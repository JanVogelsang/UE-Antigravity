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
$TargetProjectDir = $TargetProjectDir.TrimEnd('\')

Write-Host "Installing to: $TargetProjectDir"

$AgentFrameworkPluginDir = "$TargetProjectDir\.agents\plugins\UnrealEngine"

# 1. Setup AgentFramework plugin directory
Write-Host "Setting up AgentFramework..."
if (-not (Test-Path $AgentFrameworkPluginDir)) {
    New-Item -ItemType Directory -Force -Path $AgentFrameworkPluginDir | Out-Null
}
$ResolvedPluginDir = (Resolve-Path $PluginDir).Path
$ResolvedDestDir = (Resolve-Path $AgentFrameworkPluginDir).Path
if ($ResolvedPluginDir -ne $ResolvedDestDir) {
    Copy-Item -Path "$PluginDir\*" -Destination $AgentFrameworkPluginDir -Recurse -Force
}
# 2.5 Setup Workspace AGENTS.md
Write-Host "Setting up AGENTS.md at workspace root..."
$TargetAgentsPath = "$TargetProjectDir\AGENTS.md"
$SourceAgentsPath = "$PluginDir\AGENTS.md"
if (-not (Test-Path $SourceAgentsPath)) {
    $SourceAgentsPath = "$AgentFrameworkPluginDir\AGENTS.md"
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
Write-Host "  [0] AgentFramework 2.0 (default)"
Write-Host "  [1] Kilo Code"
Write-Host "  [2] Claude Code"
$clientChoice = Read-Host "Select (press Enter for AgentFramework 2.0)"
$useKiloCode = ($clientChoice -eq "1")
$useClaudeCode = ($clientChoice -eq "2")

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
$VenvDir = "$AgentFrameworkPluginDir\ExternalServer\.venv"
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
& $VenvPython -m pip install -r "$AgentFrameworkPluginDir\ExternalServer\requirements.txt"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to install Python dependencies via pip. Setup process failed."
    exit 1
}

Write-Host "Installing Bridge dependencies via pip..."
& $VenvPython -m pip install -r "$AgentFrameworkPluginDir\bridge\requirements.txt"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to install Python dependencies via pip for bridge. Setup process failed."
    exit 1
}

# Resolve repository root dynamically (parent of the UnrealEngine folder)
$RepoRoot = (Get-Item $PluginDir).Parent.FullName -replace '\\', '\\'
$PluginDirEscaped = $AgentFrameworkPluginDir -replace '\\', '\\'

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

    $SkillsSourceDir = "$AgentFrameworkPluginDir\skills"
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
      "command": "$PythonExeEscaped",
      "args": ["-X", "utf8", "-u", "-m", "bridge.main"],
      "env": {
        "BRIDGE_PROFILE": "$selectedProfile",
        "PYTHONPATH": "$PluginDirEscaped"
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
    Write-Host "  AgentFramework plugin : $AgentFrameworkPluginDir"
    Write-Host "  Kilo Code rules    : $KiloCodeRulesDir"
    Write-Host "  LLM Profile        : $selectedProfile"

} elseif ($useClaudeCode) {
    # ── Claude Code ──────────────────────────────────────────────────────────

    # Setup Claude Code skills (skill hard-links)
    $ClaudeSkillsDir = "$TargetProjectDir\.claude\skills"
    Write-Host "Setting up Claude Code skills..."
    if (-not (Test-Path $ClaudeSkillsDir)) {
        New-Item -ItemType Directory -Force -Path $ClaudeSkillsDir | Out-Null
    }

    $SkillsSourceDir = "$AgentFrameworkPluginDir\skills"
    if (-not (Test-Path $SkillsSourceDir)) { $SkillsSourceDir = "$PluginDir\skills" }

    Get-ChildItem -Path $SkillsSourceDir -Directory | ForEach-Object {
        $SkillName = $_.Name
        $SourceSkill = Join-Path $_.FullName "SKILL.md"
        $TargetSkillDir = Join-Path $ClaudeSkillsDir $SkillName
        if (-not (Test-Path $TargetSkillDir)) {
            New-Item -ItemType Directory -Force -Path $TargetSkillDir | Out-Null
        }
        $LastTargetRule = Join-Path $TargetSkillDir "SKILL.md"
        if (Test-Path $LastTargetRule) { Remove-Item $LastTargetRule }
        if (Test-Path $SourceSkill) {
            New-Item -ItemType HardLink -Path $LastTargetRule -Value $SourceSkill | Out-Null
            Write-Host "  Linked skill '$SkillName'."
        }
    }

    # Setup Workspace CLAUDE.md by copying AGENTS.md
    Write-Host "Setting up CLAUDE.md at workspace root..."
    $TargetClaudePath = "$TargetProjectDir\CLAUDE.md"
    if (Test-Path $SourceAgentsPath) {
        if (-not (Test-Path $TargetClaudePath)) {
            Copy-Item -Path $SourceAgentsPath -Destination $TargetClaudePath -Force
            Write-Host "Created CLAUDE.md at workspace root."
        } else {
            $existingContent = Get-Content -Path $TargetClaudePath -Raw
            if ($existingContent -notmatch "Agent Efficiency & Robustness Guidelines") {
                Add-Content -Path $TargetClaudePath -Value "`n`n$(Get-Content -Path $SourceAgentsPath -Raw)"
                Write-Host "Appended Agent Efficiency & Robustness Guidelines to existing CLAUDE.md."
            } else {
                Write-Host "CLAUDE.md at workspace root already contains the efficiency guidelines."
            }
        }
    }

    # Write .mcp.json
    $McpConfigPath = "$TargetProjectDir\.mcp.json"
    $McpConfigContent = @"
{
  "mcpServers": {
    "unrealengine": {
      "command": "$PythonExeEscaped",
      "args": ["-X", "utf8", "-u", "-m", "bridge.main"],
      "env": {
        "PYTHONPATH": "$PluginDirEscaped"
      }
    },
    "cpp-ast-rag": {
      "command": "$PythonExeEscaped",
      "args": ["-u", "-m", "ExternalServer.src.main"],
      "env": {
        "PYTHONPATH": "$PluginDirEscaped"
      }
    }
  }
}
"@

    if (-not (Test-Path $McpConfigPath)) {
        Set-Content -Path $McpConfigPath -Value $McpConfigContent
        Write-Host "Created .mcp.json."
    } else {
        Write-Host ".mcp.json already exists. Please manually merge the MCP server definition:"
        Write-Host $McpConfigContent
    }

    Write-Host ""
    Write-Host "Installation Complete (Claude Code)."
    Write-Host "  AgentFramework plugin : $AgentFrameworkPluginDir"
    Write-Host "  Claude config      : $McpConfigPath"
    Write-Host "  Claude rules       : $TargetClaudePath"

} else {
    # ── AgentFramework 2.0 (default) ────────────────────────────────────────────

    $McpConfigContent = @"
{
  "mcpServers": {
    "unrealengine": {
      "type": "stdio",
      "command": "$PythonExeEscaped",
      "args": ["-X", "utf8", "-u", "-m", "bridge.main"],
      "env": {
        "PYTHONPATH": "$PluginDirEscaped"
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

    $PluginConfigPath = Join-Path $AgentFrameworkPluginDir "mcp_config.json"
    Set-Content -Path $PluginConfigPath -Value $McpConfigContent
    Write-Host "Created/Updated mcp_config.json."

    Write-Host ""
    Write-Host "Installation Complete (AgentFramework 2.0)."
    Write-Host "  AgentFramework plugin : $AgentFrameworkPluginDir"
    Write-Host "  MCP config         : $PluginConfigPath"
}

# 4. Setup Git Worktree Hooks (Recommended)
Write-Host "Setting up Git Worktree Hooks..."
$TargetGithooksDir = "$TargetProjectDir\.githooks"
$SourceGithooksDir = "$PluginDir\.githooks"
if (-not (Test-Path $SourceGithooksDir)) {
    $SourceGithooksDir = "$AgentFrameworkPluginDir\.githooks"
}

if (Test-Path $SourceGithooksDir) {
    if (-not (Test-Path $TargetGithooksDir)) {
        New-Item -ItemType Directory -Force -Path $TargetGithooksDir | Out-Null
    }

    # 1. Copy the PowerShell logic script (which has a unique name post-checkout.ps1)
    $SourcePsHook = Join-Path $SourceGithooksDir "post-checkout.ps1"
    $TargetPsHook = Join-Path $TargetGithooksDir "post-checkout.ps1"
    if (Test-Path $SourcePsHook) {
        Copy-Item -Path $SourcePsHook -Destination $TargetPsHook -Force
    }

    # 2. Safely merge the bash wrapper
    $SourceBashHook = Join-Path $SourceGithooksDir "post-checkout"
    $TargetBashHook = Join-Path $TargetGithooksDir "post-checkout"
    
    $HookInvocation = "`npowershell.exe -ExecutionPolicy Bypass -File `"`$(dirname `"`$0`")/post-checkout.ps1`" `"`$1`" `"`$2`" `"`$3`""

    if (-not (Test-Path $TargetBashHook)) {
        Copy-Item -Path $SourceBashHook -Destination $TargetBashHook -Force
        Write-Host "Created post-checkout hook wrapper."
    } else {
        $ExistingContent = Get-Content -Path $TargetBashHook -Raw
        if ($ExistingContent -notmatch "post-checkout.ps1") {
            Add-Content -Path $TargetBashHook -Value $HookInvocation
            Write-Host "Appended AgentFramework worktree hook to existing post-checkout hook."
        } else {
            Write-Host "Existing post-checkout hook already contains AgentFramework configuration."
        }
    }

    # 3. Verify if target project is a Git repository
    if (Test-Path "$TargetProjectDir\.git") {
        Push-Location $TargetProjectDir
        
        # Check if they already have a custom hooksPath configured
        $CurrentHooksPath = (git config core.hooksPath) 2>$null
        if ($null -eq $CurrentHooksPath -or $CurrentHooksPath -eq "" -or $CurrentHooksPath -eq ".githooks") {
            git config core.hooksPath .githooks
            Write-Host "Configured target Git repository to use .githooks folder."
        } else {
            Write-Host "Warning: Target project already has a custom core.hooksPath configured ($CurrentHooksPath)."
            Write-Host "To enable AgentFramework worktrees, please manually add the post-checkout hook to: $CurrentHooksPath"
        }
        Pop-Location
    } else {
        Write-Host "Note: Target project does not have a local .git repository. Git hooks path was not configured automatically."
    }
}

Write-Host ""
Write-Host "------------------------------------------------------------------------"
Write-Host "Next Step (Highly Recommended):"
Write-Host "  Ensure Unreal Editor is running, open a new conversation with your"
if ($useKiloCode) {
    Write-Host "  AI assistant (which will follow the linked Kilo rules), and ask to:"
} elseif ($useClaudeCode) {
    Write-Host "  AI assistant (which will follow the linked Claude rules), and ask to:"
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

