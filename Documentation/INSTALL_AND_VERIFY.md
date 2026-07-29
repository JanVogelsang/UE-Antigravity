# Installing & Testing on a Target Project

These steps should be used whenever you need to verify that changes to the `UE-AgentFramework` repository correctly integrate with a host game. We use the local project `$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest` as our standard target project for this example.

## 1. Install the C++ Editor Plugin (`AgentFramework`)

The C++ Editor plugin handles game-thread operations and must be present in the target project's `Plugins` directory.

### Manual Developer Installation

1. Locate the target game project directory (e.g., `$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest`).
2. If it does not exist, create a `Plugins` folder at the root of the target project.
3. Copy the entire `AgentFramework` folder from the `UE-AgentFramework` repository into the target project's `Plugins` folder:
   ```powershell
   # Run from the UE-AgentFramework repository root
   Copy-Item -Path ".\AgentFramework" -Destination "$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest\Plugins\AgentFramework" -Recurse -Force
   ```
4. **Compile the Plugin**: Open the target project (`AgentFrameworkTest`) in the Unreal Engine Editor. If Unreal Engine prompts to rebuild missing modules for the `AgentFramework` plugin, choose **Yes**.

### Autonomous Agent Installation

1. **Copy Files**: Verify/create a `Plugins` directory at the target project root and copy the `AgentFramework` folder:
   ```powershell
   # Run from the UE-AgentFramework repository root
   $TargetPluginsDir = "$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest\Plugins\AgentFramework"
   if (-not (Test-Path $TargetPluginsDir)) {
       New-Item -ItemType Directory -Force -Path $TargetPluginsDir | Out-Null
   }
   Copy-Item -Path ".\AgentFramework\*" -Destination $TargetPluginsDir -Recurse -Force
   ```
2. **Headless Compilation**: Instead of launching the Editor UI to compile (which cannot be done autonomously), build the plugin headlessly using Unreal Build Tool (UBT). Locate UBT via the engine installation path in the registry, or use MSBuild if a `.sln` file is present in the target project workspace:
   ```powershell
   # Example UBT headless build command:
   # Locate UnrealEngine path (e.g., from registry HKLM:\SOFTWARE\EpicGames\Unreal Engine)
   # Or invoke the project's build batch file if available:
   & "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AgentFrameworkTestEditor Win64 Development "$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest\AgentFrameworkTest.uproject" -waitmutex
   ```

## 2. Install the Agent Plugin (`UnrealEngine`)

The Agent plugin sets up the Python AST server, the MCP JSON-RPC bridge, and agent skills.

### Manual Developer Installation

1. From the root of the `UE-AgentFramework` repository, execute the installation script:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\UnrealEngine\install.ps1
   ```
2. When the script prompts for the `target project root directory`, enter the absolute path to the target project:
   `$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest`
3. When prompted for AI assistants, select one or more targets as a comma-separated list (e.g. `0,2`), enter `all` for every assistant, or press Enter for Antigravity 2.0 only. Re-running the script later adds additional assistants without touching existing configurations.
4. The installer script will automatically:
   - Create a Python virtual environment inside the target project.
   - Install dependencies for the `bridge` proxy and `ExternalServer`.
   - Copy the MCP tools to `AgentFrameworkTest\.agents\plugins\UnrealEngine`.
   - Write the appropriate configuration file for each selected assistant (e.g. `mcp_config.json` for Antigravity, `.mcp.json` for Claude, `kilo.jsonc` for Kilo, or `.codex/config.toml` for Codex).

### Autonomous Agent Installation

1. **Non-Interactive Installation**: The `install.ps1` script utilizes interactive CLI prompts (`Read-Host`). To run it autonomously, pipe the target directory and assistant selection into standard input:
   ```powershell
   # Pipe project path (first prompt) and assistant selection (second prompt).
   # The selection may be a single option ('0'), a comma-separated list ('0,2'), or 'all'.
   # Note: selecting Kilo Code ('1') triggers a third prompt for the LLM profile.
   "$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest", "0" | powershell -ExecutionPolicy Bypass -File .\UnrealEngine\install.ps1
   ```
   This will automatically:
   - Create a Python virtual environment inside the target project.
   - Install dependencies for the `bridge` proxy and `ExternalServer`.
   - Copy the MCP tools to `AgentFrameworkTest\.agents\plugins\UnrealEngine`.
   - Write the appropriate configuration file for each selected assistant (e.g. `mcp_config.json` for Antigravity, `.mcp.json` for Claude, `kilo.jsonc` for Kilo, or `.codex/config.toml` for Codex).

## 3. Verify Integration

### Manual Developer Verification (Manual E2E Testing)

To verify the integration manually, you must open an agent conversation *inside* the target project workspace (`AgentFrameworkTest`):

1. **Start the Editor**: Ensure the `AgentFrameworkTest` Unreal Engine Editor is running (this hosts the internal C++ server on port `18777`).
2. **Initialize Workspace**: In the agent conversation for `AgentFrameworkTest`, run the initial setup prompt:
   > "Run project setup and index the project"

   This triggers the `unreal-setup` skill, updates the `unreal-instructions` skill, and creates the `project-index` skill inside `AgentFrameworkTest`.
3. **Test Queries**: Verify the Dual-MCP servers are functioning by asking the agent to perform specific Unreal Engine tasks:
   - **C++ AST Server**: *"Find the declaration for the class AAgentFrameworkTestGameModeBase"*
   - **Editor Loopback Server**: *"Extract the Blueprint variables of BP_TestBlueprint"*

### Autonomous Agent Verification (Target Workspace Verification)

To verify the integration, a conversation context must be active inside the target project workspace (`AgentFrameworkTest`). Because you cannot manually open a new UI conversation, you must either:

- Invoke a subagent scoped to the `AgentFrameworkTest` directory using your harness's subagent mechanism (in Antigravity: the `invoke_subagent` tool with `Workspace: "branch"` or `"share"`; in Claude Code: the Agent/Task tool with the target project as working directory) pointing to `$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest` (resolve to local path dynamically).
- Or, ask the user to open a conversation window inside the `AgentFrameworkTest` project workspace and run the verification commands.

Inside the target workspace, the verifying agent should run:

1. **Initialize Workspace**: Run the initial setup prompt:
   > "Run project setup and index the project"

   This triggers the `unreal-setup` skill, updates the `unreal-instructions` skill, and creates the `project-index` skill inside `AgentFrameworkTest`.
2. **Test Queries**: Verify the Dual-MCP servers are functioning by executing these tool queries:
   - **C++ AST Server**: *"Find the declaration for the class AAgentFrameworkTestGameModeBase"*
   - **Editor Loopback Server** (Ensure Unreal Editor is running on port `18777`): *"Extract the Blueprint variables of BP_TestBlueprint"*

## 4. Run Automated Integration Tests

The `UE-AgentFramework` repository contains an automated E2E test suite (`pytest`) that connects a mock client to the Python bridge and the Editor loopback server.

### Running Tests

1. **Verify Editor Status**: Ensure the `AgentFrameworkTest` Unreal Engine Editor is running (listening on port `18777`).
2. **Ensure Python Interpreter**: Ensure you are using the correct Python interpreter (usually the Windows Store app python, avoiding MSYS2).
3. **Execute Tests**: Run the automated test wrapper from the `UE-AgentFramework` root:
   ```powershell
   powershell -File .\Tests\run_tests.ps1
   ```
   This will spin up the mock client, interface with the target project's `AgentFrameworkTest` editor via port `18777`, and validate all core features (Blueprint Schema Retrieval, Graph Injection, UHT Reflection, AST Queries, etc.).
