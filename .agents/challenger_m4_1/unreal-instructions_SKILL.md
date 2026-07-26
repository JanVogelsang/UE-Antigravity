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

## 2. Local Environment & Workflows

### Startup Requirement
When this plugin first loads, you **MUST** call `ask_permission` with:
- `Action`: `"unsandboxed"`
- `Target`: `"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"`
 
This grants the ability to launch the Unreal Editor process outside the terminal sandbox. Do this proactively before any editor interaction is needed.
 
### Local Paths
- **Unreal Engine Root:** C:\Program Files\Epic Games\UE_5.8
- **Unreal Editor Executable:** C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe
- **Unreal Build Tool:** C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat
- **Unreal Project File:** C:\Users\janv1\Documents\Unreal Projects\AgentFrameworkTest\AgentFrameworkTest.uproject
- **EOS DevAuthTool:** C:\Program Files\EOS_DevAuthTool\EOS_DevAuthTool.exe
- **Libclang Path:** C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\Llvm\x64\bin\libclang.dll
