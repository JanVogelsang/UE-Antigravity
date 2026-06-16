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
$KiloCodeRulesDir = "$TargetProjectDir\.kilocode\rules"
$BridgePath = "$PluginDir\bridge.exe"

# 1. Compile Bridge if missing
if (-not (Test-Path $BridgePath)) {
    Write-Host "Compiling MCP Bridge..."
    Start-Process -FilePath "$PluginDir\src\build_bridge.bat" -WorkingDirectory "$PluginDir\src" -Wait -NoNewWindow
}

# 2. Setup Antigravity
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


# 3. Setup Kilo Code Skills
Write-Host "Setting up Kilo Code rules..."
if (-not (Test-Path $KiloCodeRulesDir)) {
    New-Item -ItemType Directory -Force -Path $KiloCodeRulesDir | Out-Null
}
$SourceSkill = "$AntigravityPluginDir\skills\unreal-workflow\SKILL.md"
if (-not (Test-Path $SourceSkill)) {
    $SourceSkill = "$PluginDir\skills\unreal-workflow\SKILL.md"
}
$TargetRule = "$KiloCodeRulesDir\unreal-workflow.md"
if (Test-Path $TargetRule) { Remove-Item $TargetRule }
New-Item -ItemType HardLink -Path $TargetRule -Value $SourceSkill | Out-Null

# 4. Setup Kilo Code MCP Config
$KiloConfigPath = "$TargetProjectDir\kilo.jsonc"
$BridgeDestPath = "$AntigravityPluginDir\bridge.exe" -replace '\\', '\\'

# Ask for LLM profile
Write-Host ""
Write-Host "Available LLM profiles:"
$profiles = Get-ChildItem -Path "$PluginDir\profiles\*.json" | ForEach-Object { $_.BaseName }
for ($i = 0; $i -lt $profiles.Count; $i++) {
    Write-Host "  [$i] $($profiles[$i])"
}
$profileChoice = Read-Host "Select a profile for Kilo Code (enter number, or press Enter for 'default')"
$selectedProfile = "default"
if (-not [string]::IsNullOrWhiteSpace($profileChoice)) {
    $idx = [int]$profileChoice
    if ($idx -ge 0 -and $idx -lt $profiles.Count) {
        $selectedProfile = $profiles[$idx]
    }
}
Write-Host "Using profile: $selectedProfile"

$McpConfigSnippet = @"
{
  "mcpServers": {
    "unrealengine": {
      "type": "stdio",
      "command": "$BridgeDestPath",
      "env": {
        "BRIDGE_PROFILE": "$selectedProfile"
      },
      "enabled": true
    }
  }
}
"@
if (-not (Test-Path $KiloConfigPath)) {
    Set-Content -Path $KiloConfigPath -Value $McpConfigSnippet
    Write-Host "Created kilo.jsonc with profile '$selectedProfile'"
} else {
    Write-Host "kilo.jsonc already exists. Please manually merge the MCP server definition:"
    Write-Host $McpConfigSnippet
}

Write-Host ""
Write-Host "Installation Complete."
Write-Host "  Antigravity plugin: $AntigravityPluginDir"
Write-Host "  Kilo Code rules:   $TargetRule"
Write-Host "  LLM Profile:       $selectedProfile"
