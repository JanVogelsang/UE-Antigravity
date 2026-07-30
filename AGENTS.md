# UE-AgentFramework - Project Overview & Agent Instructions

## Plugin Nature & Context
**IMPORTANT:** This repository contains the source code for a **plugin**, not a standalone game project. The purpose of this codebase is to be compiled and installed into *other* target Unreal Engine game projects to give them AgentFramework AI capabilities.
When working in this repository, you are developing the integration infrastructure itself.

## Architecture: Dual-MCP Server
1. **Internal C++ MCP Server (`AgentFramework/`)**: Unreal Editor plugin running an in-process HTTP loopback server on port `18777`. Executes Game Thread operations (reading Blueprint graphs, adding variables, injecting nodes). **Design Decision:** this directory strictly handles Editor integration and MCP server capabilities. It must *not* manage static agent instructions or skills.
2. **External Agent & MCP Server (`UnrealEngine/`)**: Handles all LLM logic, static instructions, and `SKILL.md` documents. Includes the Python MCP bridge `bridge/main.py`, run as `python -m bridge.main` (translates MCP JSON-RPC to HTTP loopback calls targeting the editor), and a Python AST server for C++ symbol resolution, file watching, and semantic search. Edits to the bridge require an MCP server restart to take effect, since the module is imported once per process.

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
Before writing code or utilizing MCP tools, you **MUST** read the detailed guidelines at `UnrealEngine/AGENTS.md`. That file is auto-loaded only while working under `UnrealEngine/`, so these constraints are repeated here because they also bind work in `AgentFramework/` and at the repository root:
*   **Minimal Module Dependencies Directive**: The `AgentFramework` plugin C++ code must maintain the absolute minimum set of module dependencies required for core Editor integration. If a task or feature for a host project requires additional UE feature modules (e.g., `Foliage`, `StateTree`, `GeometryCore`, `GameplayAbilities`), add that module dependency to the **host game project's `.Build.cs`**, NOT to the `AgentFramework` plugin.
*   **Ticking Guardrails**: Never place high-overhead logic or unchecked array accesses inside `Tick` or `NativeTick` functions.
*   **AST Updates**: Whenever new C++ files or headers are added, invoke `generate_compile_commands` to regenerate `compile_commands.json` so the Python AST server remains accurate.
*   **Blueprint Node Generation**: Always include a unique, valid `NodeGuid` (32-char hex string) when generating T3D representations.
*   **Automation Test Run Safeguards**: Semicolon-chain `; Quit` to test commands to avoid hanging standalone processes.

### 4. Target Project Context vs. Plugin Context
Do not confuse the UE-AgentFramework plugin development context with the context of the game project using it. **When operating inside a target game project**, the auto-generated `project-index` skill (`.agents/skills/project-index/`) is used to understand the host game's gameplay specifications and systems. Do not look for a `project-index` skill to understand the UE-AgentFramework plugin architecture itself.

### 5. Installing & Testing on a Target Project
See `Documentation/INSTALL_AND_VERIFY.md` for the full install, verification, and E2E test procedure against the standard `AgentFrameworkTest` target project. Read it whenever you need to verify that changes here integrate correctly with a host game.

### 6. Build & Deployment Hygiene (DLL Locks & Path Portability)

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
