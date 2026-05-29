---
name: unreal-workflow
description: Best practices for interacting with the Unreal Engine Editor via Antigravity MCP.
---
# Unreal Workflow Skill

1. **Editor State**: All Antigravity tools require the Unreal Engine Editor to be open. If a tool returns a "connection refused" or "Editor is not running" error, politely ask the user to start their Unreal Engine project.
2. **Tool Discovery**: Tools are dynamically fetched from the active UE project. **DO NOT waste tokens discovering or reading JSON schemas for standard tools.** Trust that native MCP tools (`spawn_actor`, `search_assets`, `inject_blueprint_nodes_t3d`, etc.) are available and use them directly.
3. **Bridge Compilation & Verification**: Before executing any Unreal Engine task, verify that the native `unrealengine` MCP tools are active. If the bridge is offline or missing:
   - You **MUST** automatically execute the build script `src/build_bridge.bat` (located inside the `UnrealEngine` plugin directory) without asking first. Propose this to the user as a safe, one-time operation to initialize the bridge.
   - After a successful compilation, you **MUST** ask the user to restart "Antigravity" specifically to allow the newly compiled bridge to be loaded.
   - If the compilation fails or the bridge remains offline, you **MUST** loudly fail the Unreal Engine task.
   - Do **NOT** attempt to manually execute or run `bridge.exe` directly via terminal shell commands.
   - Do **NOT** attempt to bypass the offline bridge by running direct shell commands (e.g., PowerShell `Invoke-RestMethod` or `curl`).

## Standard Operating Procedures (SOPs)

1. **SOP: Placing / Spawning an Object in the Level**
   - **Step 1**: Identify the asset class you want to spawn (e.g., StaticMeshComponent, a specific Blueprint).
   - **Step 2**: Use `search_assets` with a `class_filter` to find the exact path. Do NOT use `list_directory`.
   - **Step 3**: Use the native `spawn_actor` tool to place the object in the world.
   - **Step 4**: DO NOT attempt to write Python scripts or use console commands to place objects in the level.

2. **SOP: Modifying a Blueprint Graph**
   - **Step 1**: Use `get_blueprint_info` to get the current graph layout, variable names, and node names. Never guess variable names.
   - **Step 2**: Use `inject_blueprint_nodes_t3d` to insert new logic in T3D format.
   - **Step 3**: If pins need connecting, use `connect_blueprint_pins`.
   - **Step 4**: Run `compile_blueprint` to verify.

3. **SOP: C++ vs Blueprint / Python Priority**
   - Unless the user explicitly demands a C++ implementation, always default to Blueprint manipulation (`inject_blueprint_nodes_t3d`) or Python utility scripts (`execute_python_script`). Modifying C++ requires costly live coding recompiles and should be minimized.

4. **SOP: Asset File Management (Moving, Renaming, Deleting)**
   - **CRITICAL**: Never use standard terminal shell commands (`mv`, `rm`, `del`) to move or rename Unreal Engine assets (.uasset/.umap) on disk. This will corrupt the project's internal asset registry.
   - **Step 1**: Always use `execute_python_script` and utilize the `unreal.EditorAssetLibrary` functions (`rename_asset`, `delete_asset`, `duplicate_asset`) to handle asset operations safely.

5. **SOP: Play-In-Editor (PIE) Safety**
   - **CRITICAL**: Before attempting to compile C++, edit Blueprints, or inject T3D nodes, you must verify the game is not running. If a PIE session is active, call `stop_pie_session` first.

6. **SOP: Skipping Schema Discovery (Token Conservation)**
   - **CRITICAL**: If you are performing a standard workflow (e.g., spawning an actor, modifying a Blueprint, finding an asset), **DO NOT waste tokens viewing schema JSON files**.
   - Trust that native MCP tools like `spawn_actor`, `search_assets`, and `inject_blueprint_nodes_t3d` are available and use them directly according to their standard schemas. Only read schema files if you are using an unfamiliar tool for the very first time.

7. **SOP: Asset Path Formatting**
   - **CRITICAL**: Whenever an Unreal Engine tool requires an `asset_path`, it expects the internal Unreal package path (e.g., `/Game/Blueprints/BP_MyActor`), **NOT** a Windows file system path (e.g., `C:/Projects/.../BP_MyActor.uasset`).
   - If you only have a filesystem path, use `search_assets` with the basename to discover the correct `/Game/...` asset path before calling tools like `spawn_actor` or `set_component_properties`.



