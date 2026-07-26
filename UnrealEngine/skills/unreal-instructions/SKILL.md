---
name: unreal-instructions
description: REQUIRED ENTRY POINT for ALL Unreal Engine tasks. Must trigger for UE5, Blueprints, C++, UMG, Editor, UObject, multiplayer, compilation, or any engine interaction.
---
# Unreal Engine MCP Guide

## 1. Dual-MCP Architecture & Tool Routing

-   **`unrealengine` (Internal Editor, port 18777)**: Use for Blueprint/UMG/Level changes and asset manipulation (e.g., `spawn_actor`, `inject_blueprint_nodes_t3d`).
    -   *Constraint*: ALWAYS use Unreal paths (`/Game/...`). NEVER use Windows paths or shell commands (`rm`, `mv`) for `.uasset` files.
    -   *UMG Tool Parameters*: When using native tools to edit complex widget layouts (like `set_widget_slot`), be aware that layout parameters (anchors, offsets, alignment, Z-order) often must be passed inside a nested object (e.g., `slot_properties`), rather than at the top level of the tool arguments. Always check the tool schema structure carefully.
-   **`cpp-ast-rag` (External AST)**: The Python-based AST server. ALWAYS use its specialized tools (`query_cpp_ast`, `search_vector_db`, `search_similar_blueprints`) for C++ semantic lookups and documentation search instead of generic grep/file searches.
- **Compilation**: Use `trigger_compile` tool when Editor is open. NEVER run manual terminal builds (UBT/MSBuild) with an open Editor.

<!-- LOCAL_ENV_START -->
## 2. Local Environment & Workflows

### Startup Requirement (Sandbox Pre-Authorization)
Launching the Unreal Editor spawns a long-lived GUI process outside the terminal sandbox. How to authorize this depends on your agent harness:
- **If your harness provides an `ask_permission` tool (e.g. Antigravity):** When this plugin first loads, proactively call `ask_permission` with `Action`: `"unsandboxed"` and `Target`: `"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"` before any editor interaction is needed.
- **If it does not (e.g. Claude Code, OpenAI Codex, Kilo Code):** Skip this step entirely — do NOT search for or attempt to call `ask_permission`. Launching the editor goes through your harness's standard permission flow: the launch command itself will prompt for approval unless the installer already pre-approved it (e.g. via `.claude/settings.json` allow rules or your assistant's command allowlist).

### Local Paths
- **Unreal Engine Root:** C:\Program Files\Epic Games\UE_5.8
- **Unreal Editor Executable:** C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe
- **Unreal Build Tool:** C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat
- **Unreal Project File:** C:\Users\janv1\Documents\Unreal Projects\AgentFrameworkTest\AgentFrameworkTest.uproject
- **EOS DevAuthTool:** C:\Program Files\EOS_DevAuthTool\EOS_DevAuthTool.exe

### Developer Tool Workflows
#### 1. EOS DevAuthTool
Only start this tool when testing multiplayer/online functionality or when explicitly requested. When required:
```powershell
Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = '"C:\Program Files\EOS_DevAuthTool\EOS_DevAuthTool.exe"' }
```
Wait until the user has interacted with the opened EOS_DevAuthTool window (e.g. to log in, accept scopes, or configure credentials) before trying to use it for authentication.

#### 2. Building the Project
To compile the C++ code and binaries for the editor (e.g. when editor is closed, to prevent out-of-date binaries preventing launch):
```powershell
Start-Process -FilePath "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -ArgumentList "AgentFrameworkTestEditor", "Win64", "Development", '"C:\Users\janv1\Documents\Unreal Projects\AgentFrameworkTest\AgentFrameworkTest.uproject"', "-WaitMutex" -Wait -NoNewWindow
```

#### 3. Launching the Unreal Editor
##### A. Standard Launch (Default / Single-player / Local Testing)
Launch the editor without online/EOS arguments (DevAuthTool is not required):
```powershell
Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = '"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\janv1\Documents\Unreal Projects\AgentFrameworkTest\AgentFrameworkTest.uproject"' }
```

##### B. Multiplayer / EOS Launch (Only when testing multiplayer or explicitly requested)
Start the EOS DevAuthTool first, then launch the editor with developer authentication arguments:
```powershell
Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = '"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\janv1\Documents\Unreal Projects\AgentFrameworkTest\AgentFrameworkTest.uproject" -CustomConfig=EOS -AUTH_TYPE=developer -AUTH_LOGIN=localhost:8080 -AUTH_PASSWORD=TauDev' }
```
<!-- LOCAL_ENV_END -->

## 3. Skills Directory
Read the corresponding file with your harness's native file-reading tool (`view_file` in Antigravity, `Read` in Claude Code, or equivalent) when performing these tasks:
- [blueprint-authoring](../blueprint-authoring/SKILL.md): Modifying `.uasset` blueprints (nodes, variables, formatting).
- [setup-replication](../setup-replication/SKILL.md): Network replication, RPCs, and RepNotify.
- [add-component](../add-component/SKILL.md): Declaring/attaching UActorComponents in C++.
- [setup-input](../setup-input/SKILL.md): Enhanced Input IMCs, Actions, and bindings.
- [niagara-authoring](../niagara-authoring/SKILL.md): Niagara VFX creation/modification.
- [unreal-testing-sops](../unreal-testing-sops/SKILL.md): Automated UI, performance testing, and PIE SOPs.
- [create-actor](../create-actor/SKILL.md): Boilerplate for new Actor/Pawn C++ classes.
- [create-interface](../create-interface/SKILL.md): Blueprint and C++ interface creation.
- [pie-verifier](../pie-verifier/SKILL.md): Play-In-Editor (PIE) state and viewport checks.


