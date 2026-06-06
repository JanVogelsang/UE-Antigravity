---
name: unreal-workflow
description: Best practices for interacting with the Unreal Engine Editor via Antigravity MCP.
---
# Unreal Workflow Skill

1. **Editor State**: All Antigravity tools require the Unreal Engine Editor to be open. If a tool returns a "connection refused" or "Editor is not running" error, start the editor.
   - **How to Start the Editor**: Run this portable PowerShell command in the terminal to resolve the engine path from the registry and start the editor with the required developer authentication parameters:
     ```powershell
     $RegistryPath = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.7"
     $InstalledDir = (Get-ItemProperty -Path $RegistryPath -Name InstalledDirectory -ErrorAction SilentlyContinue).InstalledDirectory
     if (-not $InstalledDir) { $InstalledDir = "C:\Program Files\Epic Games\UE_5.7" }
     $EditorExe = Join-Path $InstalledDir "Engine\Binaries\Win64\UnrealEditor.exe"
     $ProjectFile = Join-Path (Get-Location) "Tau.uproject"
     Start-Process -FilePath $EditorExe -ArgumentList "`"$ProjectFile`"", "-CustomConfig=EOS", "-AUTH_TYPE=developer", "-AUTH_LOGIN=localhost:8080", "-AUTH_PASSWORD=TauDev"
     ```
2. **Tool Discovery**: Tools are dynamically fetched from the active UE project. **DO NOT waste tokens discovering or reading JSON schemas for standard tools.** Trust that native MCP tools (`spawn_actor`, `search_assets`, `inject_blueprint_nodes_t3d`, etc.) are available and use them directly.
3. **Bridge Compilation & Verification**: Before executing any Unreal Engine task, verify that the native `unrealengine` MCP tools are active. If the bridge is offline or missing:
   - You **MUST** automatically execute the build script `src/build_bridge.bat` (located inside the `UnrealEngine` plugin directory) without asking first. This script compiles the bridge and automatically generates the machine-specific `unreal-env` skill. Propose this to the user as a safe, one-time operation to initialize the bridge.
   - After a successful compilation, you **MUST** ask the user to restart "Antigravity" specifically to allow the newly compiled bridge and the dynamically generated `unreal-env` skill to be loaded.
   - If the `unreal-env` skill is missing or outdated but the bridge is active, you can regenerate it by running `powershell -ExecutionPolicy Bypass -File .agents/plugins/UnrealEngine/src/generate_env_skill.ps1`.
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

8. **SOP: Data Asset Creation and Authoring**
   - **Step 1**: Use `create_data_asset` to instantiate a data asset. Provide the full package path and class name (use `_C` suffix for Blueprint-defined classes).
   - **Step 2**: Use `get_data_asset_info` if you need to inspect the available fields on an existing Data Asset.
   - **Step 3**: Use `set_data_asset_properties` to populate fields. Parse values as strings exactly matching C++ reflection formats (e.g., vectors as `"(X=100.0,Y=0.0,Z=2.0)"`, soft asset paths for assets).
   - **Best Practice**: Prefer **Soft References** (`TSoftObjectPtr`) for heavy assets (meshes, textures, sound cues) when creating/modifying Data Asset definitions to avoid cascade loading memory bottlenecks.
   - **Rule**: Never use Python scripts to modify Data Assets if these native tools are available.

9. **SOP: Visual UI Design**
   - **CRITICAL**: Do not build complex UIs blindly. Use the `capture_widget` tool to visually see the widgets you create.
   - **Step 1**: Use `create_widget_blueprint` and `add_widget` to construct the widget hierarchy.
   - **Step 2**: Call `capture_widget` with the widget's `asset_path`. The tool will return a Base64 image artifact directly in the chat.
   - **Step 3**: Visually analyze the Base64 image artifact. Check alignments, padding, and colors.
   - **Step 4**: Iteratively adjust properties using `set_widget_slot` and `set_widget_property`, re-capturing as necessary until the layout perfectly matches the desired design.
