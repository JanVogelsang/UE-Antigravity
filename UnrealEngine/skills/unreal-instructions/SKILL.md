---
name: unreal-instructions
description: Global instructions for agent interactions with the Unreal Editor MCP.
---
# Unreal Engine MCP - Global Instructions

You are connected to an Unreal Engine instance via the Antigravity MCP plugin. Whenever you perform actions in this ecosystem, you MUST obey these core rules. Think of these as the "laws of physics" for this workspace.

## 1. Asset Pathing
- **Always use Unreal Engine paths:** Paths must start with `/Game/...` (e.g., `/Game/Blueprints/BP_MyCharacter`).
- **Never use Windows paths for assets:** Do NOT use `C:/...` or relative file paths to reference `.uasset` files when using UE tools.
- **Reference format:** When an exact reference is needed by the engine, use the formal path format if required (e.g., `Blueprint'/Game/Path/To/Asset.Asset'`).

## 2. Implementation Strategy (C++ vs Blueprint)
- **Identify Project Type First:** Verify whether the target project is a C++ project (contains a `Source/` directory and `.Build.cs` / `.Target.cs` files) or a Blueprint-only project. If it is Blueprint-only, you MUST NOT write C++ code or create source files, as it will break the project unless the user has compiler tools installed. Implement all logic through Blueprint tools or Python.
- **Prefer C++ for Logic (in C++ Projects):** As an AI, you are much better at reading, writing, and reasoning about text-based C++ than generating verbose Blueprint T3D node graphs. Default to C++ for core logic, systems, and data structures.
- **Use Blueprints for Wiring/UI:** Use Blueprints primarily for assembling components, UI layouts, or simple event wiring.
- **C++ Compilation Workflow:**
  - **If the Editor is Closed:** You are free to make extensive changes to headers (`.h`), adding `UPROPERTY` and `UFUNCTION` macros as needed. Regular compilation can be used without worrying about Live Coding restrictions.
  - **If the Editor is Running:**
    - **Implementation (.cpp):** You can modify `.cpp` files freely and then trigger a compile yourself using the `trigger_compile` tool for immediate testing. **Do NOT run manual compile commands in the terminal (like running UnrealBuildTool.exe directly) if the editor is open; you must use the `trigger_compile` tool.**
    - **Headers (.h):** Modifying header files with new UHT macros (`UPROPERTY`/`UFUNCTION`) cannot be safely Live Coded. Batch your header changes together, and inform the user that they must close the editor and perform a full rebuild before the changes can be safely tested. Do not avoid UPROPERTY/UFUNCTION changes; just plan them efficiently to minimize restarts!

## 3. Safety and Editor State
- **Verify PIE is Stopped:** Before creating, modifying, or deleting assets, ensure that Play-In-Editor (PIE) is stopped. Editing assets while the game is running can cause crashes or corrupt data.
- **No File System Hacks:** NEVER use terminal/shell commands (like `rm`, `mv`, `del`) to move, rename, or delete `.uasset` or `.umap` files directly on disk. Always use the provided Unreal Engine tools. Modifying assets via the file system bypasses UE's Asset Registry and will corrupt project references.
- **Never Kill or Close the Editor Automatically:** If the editor needs to be closed or restarted (e.g., for full header rebuilds, Live Coding limits, or updating plugin binaries), you MUST NOT close or kill the editor process yourself. Instead, explain the situation to the user and request that they perform the restart for you (e.g., via Rider or by manually restarting the editor).
- **Never Kill Build Processes:** You MUST NEVER run `taskkill` or other command-line utilities to force-terminate build processes (such as `UnrealBuildTool.exe`, `cl.exe`, `link.exe`, `msbuild.exe`, etc.) to resolve compilation conflicts or lock errors (e.g., `UnrealBuildTool_Mutex` conflicts). These conflicts are transient. If a build is blocked by a running instance, wait for it to complete and retry, or gracefully notify the user.
- **Prefer trigger_compile Over Terminal Builds:** When the Unreal Editor is open, always use the `trigger_compile` tool to run builds. Do NOT trigger compiler executions via `run_command` in the terminal when the editor is running.

## 4. UI and Visual Design
- **Use Your Eyes:** When building or debugging UMG widgets or visual elements, use your vision capabilities (e.g., `capture_widget`) to evaluate the design. Do not blindly guess visual layouts without verifying the result visually.

## 5. Error Recovery and Troubleshooting
- **Validate Paths:** If a tool fails with an "asset not found" error, verify the exact path using asset listing/search tools before retrying.
- **Check Editor Logs:** If a command fails or behaves unexpectedly, use the available tools to check the Unreal Engine Output Log for detailed error messages and warnings.

## 6. C++ Standards and Unreal Engine Rules
- **Strict Naming Prefixes:** You MUST follow Unreal Engine naming conventions for C++ types. UHT will fail to compile if prefixes are incorrect:
  - `A` for Actors (e.g., `AMyCharacter`)
  - `U` for UObjects/Components (e.g., `UMyComponent`, `UUserWidget`)
  - `F` for Structs (e.g., `FMyStruct`)
  - `E` for Enums (e.g., `EMyEnum`)
  - `I` for Interfaces (e.g., `IMyInterface`)
- **Generated Headers (.generated.h):** 
  - Every header containing a `UCLASS`, `USTRUCT`, `UINTERFACE`, or `UENUM` must include the generated header as the **last** include: `#include "FileName.generated.h"`.
  - The name must exactly match the header's filename.
- **Garbage Collection Safety:** 
  - Any raw pointers to `UObject` types (including `AActor`, `UActorComponent`, etc.) must be marked with the `UPROPERTY()` macro if they are stored as class members. If omitted, they are not tracked by the reflection system and will be Garbage Collected, causing random dangling pointer crashes.
- **Module Dependencies (.Build.cs):**
  - If you use classes from specific systems (e.g., `UUserWidget` from `UMG`, GAS classes from `GameplayAbilities`, input classes from `EnhancedInput`), you must add the respective module names to the `PublicDependencyModuleNames` or `PrivateDependencyModuleNames` in the project's `<ModuleName>.Build.cs` file. Failure to do so will cause compiler/linker errors.

## 7. Tool Usage and Skill Selection
- **Prioritize Domain-Specific Tools:** Always choose the most specific tool domain (e.g., `blueprint_tools`, `widget_tools`, `cpp_tools`, `niagara_tools`) over generic file writing or terminal commands. Never use raw shell commands to perform actions that have dedicated MCP tools.
- **Prefer Macro Tools:** Use high-level Macro tools (such as `macro_create_cpp_class`) over low-level ones when creating assets or files, as they automatically handle complex boilerplate (like API export macros and UHT setup) and reduce errors.
- **Proactively Use Skills:**
  - Before executing common Unreal Engine tasks (such as setting up replication, creating actors, or adding components), check the `Antigravity/Resources/Skills/` folder for any relevant `.md` skill templates.
  - If a matching skill file exists, read its contents and follow its steps as your primary execution guideline.
- **Read Schemas for Constraints:** When using a tool for the first time, check the schema in `Antigravity/Resources/ToolSchemas/` to understand default values, constraint ranges, and required fields to prevent failed tool execution.

## 8. Standard Operating Procedures (SOPs)

1. **SOP: Placing / Spawning an Object in the Level**
   - **Step 1**: Identify the asset class you want to spawn (e.g., StaticMeshComponent, a specific Blueprint).
   - **Step 2**: Use `search_assets` with a `class_filter` to find the exact path. Do NOT use `list_directory`.
   - **Step 3**: Use the native `spawn_actor` tool to place the object in the world.
   - **Step 4**: DO NOT attempt to write Python scripts or use console commands to place objects in the level.

2. **SOP: Modifying a Blueprint Graph**
   - Refer to the dedicated [blueprint-authoring](file:///C:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/UnrealEngine/skills/blueprint-authoring/SKILL.md) skill. Follow its detailed 5-step SOP covering state queries, batch operations, T3D generation rules (including critical `NodeGuid` assignment), layout formatting, and pin auditing.

3. **SOP: Asset File Management (Moving, Renaming, Deleting)**
   - **CRITICAL**: Never use standard terminal shell commands (`mv`, `rm`, `del`) to move or rename Unreal Engine assets (.uasset/.umap) on disk. This will corrupt the project's internal asset registry.
   - **Step 1**: Before writing custom one-off scripts for standard batch operations, check the pre-built macro scripts library in the plugin's `src/scripts/` directory (e.g., `clean_naming_conventions.py`, `organize_assets_by_type.py`, `find_unreferenced_assets.py`, or `bulk_replace_references.py`).
   - **Step 2**: If no pre-built macro script exists for the specific task, write and run a custom Python script via `execute_python_script` utilizing `unreal.EditorAssetLibrary` functions (`rename_asset`, `delete_asset`, `duplicate_asset`) to handle asset operations safely.

4. **SOP: Skipping Schema Discovery (Token Conservation)**
   - **CRITICAL**: If you are performing a standard workflow (e.g., spawning an actor, modifying a Blueprint, finding an asset), **DO NOT waste tokens viewing schema JSON files**.
   - Trust that native MCP tools like `spawn_actor`, `search_assets`, and `inject_blueprint_nodes_t3d` are available and use them directly according to their standard schemas. Only read schema files if you are using an unfamiliar tool for the very first time.

5. **SOP: Data Asset Creation and Authoring**
   - **Step 1**: Use `create_data_asset` to instantiate a data asset. Provide the full package path and class name (use `_C` suffix for Blueprint-defined classes).
   - **Step 2**: Use `get_data_asset_info` if you need to inspect the available fields on an existing Data Asset.
   - **Step 3**: Use `set_data_asset_properties` to populate fields. Parse values as strings exactly matching C++ reflection formats (e.g., vectors as `"(X=100.0,Y=0.0,Z=2.0)"`, soft asset paths for assets).
   - **Best Practice**: Prefer **Soft References** (`TSoftObjectPtr`) for heavy assets (meshes, textures, sound cues) when creating/modifying Data Asset definitions to avoid cascade loading memory bottlenecks.
   - **Rule**: Never use Python scripts to modify Data Assets if these native tools are available.

6. **SOP: Visual UI Design**
   - **CRITICAL**: Do not build complex UIs blindly. Use the `capture_widget` tool to visually see the widgets you create.
   - **Step 1**: Use `create_widget_blueprint` to create the asset, then use `instantiate_ui_hierarchy` to construct the entire widget tree hierarchy, properties, slots, fonts, brushes, and event bindings in a single transaction and single compilation.
   - **Step 2**: Call `capture_widget` with the widget's `asset_path`. The tool will return a Base64 image artifact directly in the chat.
   - **Step 3**: Visually analyze the Base64 image artifact. Check alignments, padding, and colors.
   - **Step 4**: Iteratively adjust properties using `instantiate_ui_hierarchy` or specific setters, re-capturing as necessary until the layout perfectly matches the desired design.

## 9. Routing & Sub-Agent Workflow Rules

### Agent Lite Routing (MANDATORY)
To conserve tokens and maintain task velocity, the Main Agent must route tasks efficiently:
- **Direct Thread Execution (Lite):** For simple read-only queries (e.g., "how does this Blueprint work?"), simple value edits (e.g., `set_node_pin_default`), or variable renaming, the Main Agent **MUST** run the task directly without spawning sub-agents.
- **Spawning Sub-agents (Telos Pipeline):** Only delegate to the sub-agent pipeline when the task involves multi-file structural changes, new gameplay logic systems, complex math graph building, or multi-step Blueprint asset creation.

### Sub-Agent Workflow for Blueprint Authoring (Antigravity 4-Layer Context)
- For complex Blueprint authoring involving multi-file structural changes, new gameplay logic systems, complex math graphs, or multi-step Blueprint asset creation, the Main Agent **MUST** coordinate the specialized `Blueprint Architect`, `Blueprint Engineer`, and `QA Auditor` sub-agents. 
- Refer to the dedicated [blueprint-authoring](file:///C:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/UnrealEngine/skills/blueprint-authoring/SKILL.md) skill for details on sub-agent roles, the mandatory Gap Analysis protocol, and the routing protocol.
