# UE-AgentFramework - Project Overview & Agent Instructions

This document provides a high-level overview of the UE-AgentFramework repository, its architectural design decisions, and common workflows. All future agents working in this repository should refer to this document to understand the setup and constraints.

## Plugin Nature & Context
**IMPORTANT:** This repository contains the source code for a **plugin**, not a standalone game project. The purpose of this codebase is to be compiled and installed into *other* target Unreal Engine game projects to give them AgentFramework AI capabilities.
When working in this repository, you are developing the integration infrastructure itself.

## Purpose & Architecture
UE-AgentFramework is a high-performance integration that connects the Google AgentFramework 2.0 AI coding assistant directly to a running Unreal Engine Editor. It operates using a **Dual-MCP Server** architecture:

1. **Internal C++ MCP Server (`AgentFramework/`)**: An Unreal Engine Editor plugin running an in-process HTTP loopback server on port `18777`. It executes Game Thread operations like reading Blueprint graphs, adding variables, and injecting nodes. **Design Decision:** This directory strictly handles Editor integration and MCP server capabilities. It must *not* manage static agent instructions or skills.
2. **External Agent & MCP Server (`UnrealEngine/`)**: The AgentFramework agent plugin. It handles all LLM logic, static instructions, and `SKILL.md` documents. It includes a `bridge.exe` that translates MCP JSON-RPC messages to HTTP loopback calls targeting the editor, and a Python AST server for C++ symbol resolution, file watching, and semantic search.

## Repository Setup
*   **`AgentFramework/`**: C++ plugin for the Unreal Editor.
*   **`UnrealEngine/`**: Agent plugin, MCP configurations, bridge proxy, Python AST server, and static skills. Contains its own `AGENTS.md` with detailed coding guidelines for agents working inside the target game projects.
*   **`Documentation/`**: Contains core system documents. Agents should read `PROJECT.md` (architecture & milestones), `DEVELOPMENT.md` (environment & build rules), and `TEST_INFRA.md` (E2E testing matrix).
*   **`Tests/`**: Automated integration and unit tests (`pytest` based) testing the Dual-MCP communication flow against a target game project.

## Common Workflows

### 1. Building the Plugin
Unreal Engine compilation relies on UBT. Running builds concurrently can trigger a conflicting UAT instance error. Always run the build script from the repository root using the UAT mutex bypass:
```powershell
$env:uebp_UATMutexNoWait = '1'
powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip
```
This builds the plugin and usually copies the binaries to the target test game project.

### 2. Running Automated Tests
Always use the PowerShell wrapper to ensure the correct Python interpreter (which has `pip` and `pytest`) is used, avoiding conflicts with MSYS2 environments.
*   To run Python tests: `powershell -File .\Tests\run_tests.ps1`
*   To run all C++ and Python tests sequentially: `powershell -File .\Tests\run_all_tests.ps1`

### 3. Agent Coding Guidelines & Constraints
Before writing code or utilizing MCP tools, you **MUST** read the detailed guidelines located at `UnrealEngine/AGENTS.md`. Key highlights include:
*   **Tool Priority**: Prefer native Unreal Engine MCP tools over executing Python scripts.
*   **AST Updates**: Whenever new C++ files or headers are added, invoke `generate_compile_commands` to regenerate `compile_commands.json` so the Python AST server remains accurate.
*   **Blueprint Node Generation**: Always include a unique, valid `NodeGuid` (32-char hex string) when generating T3D representations.
*   **The "Ponytail" Ladder**: Follow strict evaluation before writing new C++ or Blueprint logic.
*   **Automation Test Run Safeguards**: Semicolon-chain `; Quit` to test commands to avoid hanging standalone processes.
*   **Minimal Module Dependencies Directive**: The `AgentFramework` plugin C++ code must maintain the absolute minimum set of module dependencies required for core Editor integration. If a specific task or feature being implemented for a host project requires additional UE feature modules (e.g., `Foliage`, `StateTree`, `GeometryCore`, `GameplayAbilities`), the agent MUST add that module dependency to the **host game project's `.Build.cs`**, NOT to the `AgentFramework` plugin.
*   **Ticking Guardrails**: Never place high-overhead logic or unchecked array accesses inside `Tick` or `NativeTick` functions.

### 4. Target Project Context vs. Plugin Context
Do not confuse the UE-AgentFramework plugin development context with the context of the game project using it.
*   **When developing the plugin (this repository):** Refer to `Documentation/PROJECT.md` and `Documentation/DEVELOPMENT.md` for architectural context.
*   **When operating inside a target game project:** The auto-generated `project-index` skill (`.agents/skills/project-index/`) is used to understand the host game's gameplay specifications and systems. Do not look for a `project-index` skill to understand the UE-AgentFramework plugin architecture itself.

## 5. Installing & Testing on a Target Project

These steps should be used whenever you need to verify that changes to the `UE-AgentFramework` repository correctly integrate with a host game. We use the local project `$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest` as our standard target project for this example.

### 5.1 Install the C++ Editor Plugin (`AgentFramework`)

The C++ Editor plugin handles game-thread operations and must be present in the target project's `Plugins` directory.

#### Manual Developer Installation
1. Locate the target game project directory (e.g., `$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest`).
2. If it does not exist, create a `Plugins` folder at the root of the target project.
3. Copy the entire `AgentFramework` folder from the `UE-AgentFramework` repository into the target project's `Plugins` folder:
   ```powershell
   # Run from the UE-AgentFramework repository root
   Copy-Item -Path ".\AgentFramework" -Destination "$env:USERPROFILE\Documents\Unreal Projects\AgentFrameworkTest\Plugins\AgentFramework" -Recurse -Force
   ```
4. **Compile the Plugin**: Open the target project (`AgentFrameworkTest`) in the Unreal Engine Editor. If Unreal Engine prompts to rebuild missing modules for the `AgentFramework` plugin, choose **Yes**.

#### Autonomous Agent Installation
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

### 5.2 Install the Agent Plugin (`UnrealEngine`)

The Agent plugin sets up the Python AST server, the MCP JSON-RPC bridge, and agent skills.

#### Manual Developer Installation
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

#### Autonomous Agent Installation
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

### 5.3 Verify Integration

#### Manual Developer Verification (Manual E2E Testing)
To verify the integration manually, you must open an agent conversation *inside* the target project workspace (`AgentFrameworkTest`):
1. **Start the Editor**: Ensure the `AgentFrameworkTest` Unreal Engine Editor is running (this hosts the internal C++ server on port `18777`).
2. **Initialize Workspace**: In the agent conversation for `AgentFrameworkTest`, run the initial setup prompt:
   > "Run project setup and index the project"
   This triggers the `unreal-setup` skill, updates the `unreal-instructions` skill, and creates the `project-index` skill inside `AgentFrameworkTest`.
3. **Test Queries**: Verify the Dual-MCP servers are functioning by asking the agent to perform specific Unreal Engine tasks:
   - **C++ AST Server**: *"Find the declaration for the class AAgentFrameworkTestGameModeBase"*
   - **Editor Loopback Server**: *"Extract the Blueprint variables of BP_TestBlueprint"*

#### Autonomous Agent Verification (Target Workspace Verification)
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

### 5.4 Run Automated Integration Tests

The `UE-AgentFramework` repository contains an automated E2E test suite (`pytest`) that connects a mock client to the Python bridge and the Editor loopback server.

#### Running Tests
1. **Verify Editor Status**: Ensure the `AgentFrameworkTest` Unreal Engine Editor is running (listening on port `18777`).
2. **Ensure Python Interpreter**: Ensure you are using the correct Python interpreter (usually the Windows Store app python, avoiding MSYS2).
3. **Execute Tests**: Run the automated test wrapper from the `UE-AgentFramework` root:
   ```powershell
   powershell -File .\Tests\run_tests.ps1
   ```
   This will spin up the mock client, interface with the target project's `AgentFrameworkTest` editor via port `18777`, and validate all core features (Blueprint Schema Retrieval, Graph Injection, UHT Reflection, AST Queries, etc.).

### 5.5 Build & Deployment Hygiene (DLL Locks & Path Portability)

When deploying compiled plugin binaries or files to target projects, adhere to the following:

1. **Editor DLL File Locks:** 
   The Unreal Editor locks the compiled plugin DLLs (`UnrealEditor-AgentFrameworkActions.dll`, etc.) while running. Any deployment script, build step, or manual `Copy-Item` command will fail to overwrite these binaries if the target project's Editor is open.
   *   **Rule:** Before verifying new plugin features in a live editor, always ensure the Editor was **fully closed** when the binaries were copied.
   *   **Rule:** If a copy step fails with an "Access Denied" or "Used by another process" error, explicitly ask the user to close the Editor and retry.

2. **Dynamic Home Directory Resolution:**
   Avoid using hardcoded user home directories (e.g. `C:\Users\Jan`) in scripts, configurations, or paths.
   *   **Rule:** Always resolve the user profile path dynamically.
       *   In PowerShell: Use `$env:USERPROFILE`.
       *   In Python: Use `os.path.expanduser('~')`.
       *   In C++: Use `FPaths::ProjectDir()` or similar engine utilities where possible.

