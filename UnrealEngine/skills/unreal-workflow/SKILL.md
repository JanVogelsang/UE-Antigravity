---
name: unreal-workflow
description: Best practices for interacting with the Unreal Engine Editor via Antigravity MCP.
---
# Unreal Workflow Skill

## Agent Efficiency & Robustness Guidelines
- **Wait Durations & Scheduling**: Avoid active polling loops or shell-based sleep commands. Use the platform's native `schedule` tool (with `DurationSeconds`) when waiting for long background tasks. Ensure wait commands/timeouts for Editor start, compilation, and PIE loading are realistic (e.g., 5-15 seconds) to prevent premature check-in timeouts.
- **Log Parsing Efficiency**: When reading active logs via commands, always use `Get-Content -Path <LogPath> -Tail <N>` (PowerShell) or `tail -n <N>` (Unix/macOS) instead of reading the entire file.
- **Precision File Reading with `view_file`**: Use the native `view_file` tool rather than command-line utilities (like `cat` or `Get-Content`) to view source files. Specify `StartLine` and `EndLine` whenever possible to read only the target lines, keeping token overhead minimal.
- **Handling Failures and Stuck Processes**: If an editor session, compilation command, or automation test fails or hangs, do not retry blindly. Check the tail of the log file, report the issue clearly, and immediately ask the user for assistance or verification.


1. **Startup Permission**: When this plugin first loads, you **MUST** immediately call `ask_permission` with `Action: "unsandboxed"` and `Target` set to the **Unreal Editor Executable** path from the `unreal-env` skill. This grants the ability to launch the editor process outside the terminal sandbox. Do this proactively before any editor interaction is needed.
2. **Editor State**: All Antigravity tools require the Unreal Engine Editor to be open. If a tool returns a "connection refused" or "Editor is not running" error, start the editor.
   - **Environment Delegation**: Always refer to the `unreal-env` skill for all local environment configurations, paths, and commands needed to launch the Unreal Editor, start the EOS DevAuthTools, or build the project.
3. **Tool Discovery**: Tools are dynamically fetched from the active UE project. **DO NOT waste tokens discovering or reading JSON schemas for standard tools.** Trust that native MCP tools (`spawn_actor`, `search_assets`, `inject_blueprint_nodes_t3d`, etc.) are available and use them directly.
4. **Bridge Compilation & Verification**: Before executing any Unreal Engine task, verify that the native `unrealengine` MCP tools are active. If the bridge is offline or missing:
   - You **MUST** automatically execute the build script `src/build_bridge.bat` (located inside the `UnrealEngine` plugin directory) without asking first. This script compiles the bridge and automatically generates the machine-specific `unreal-env` skill. Propose this to the user as a safe, one-time operation to initialize the bridge.
   - After a successful compilation, you **MUST** ask the user to restart "Antigravity" specifically to allow the newly compiled bridge and the dynamically generated `unreal-env` skill to be loaded.
   - If the `unreal-env` skill is missing or outdated but the bridge is active, you can regenerate it by running `powershell -ExecutionPolicy Bypass -File .agents/plugins/UnrealEngine/src/generate_env_skill.ps1`.
   - If the compilation fails or the bridge remains offline, you **MUST** loudly fail the Unreal Engine task.
   - Do **NOT** attempt to manually execute or run `bridge.exe` directly via terminal shell commands.
   - Do **NOT** attempt to bypass the offline bridge by running direct shell commands (e.g., PowerShell `Invoke-RestMethod` or `curl`).
5. **C++ Compilation Priority (trigger_compile)**: When the Unreal Editor is running, always trigger compilation using the `trigger_compile` tool. Do NOT run manual build commands in the terminal (such as running `UnrealBuildTool.exe` directly) if the editor is open, as this causes mutex/file lock collisions. Furthermore, NEVER run `taskkill` or other command line utilities to terminate compiler or build processes (like `UnrealBuildTool.exe`, `cl.exe`, `link.exe`, etc.) to resolve concurrency lock errors (e.g., `UnrealBuildTool_Mutex` conflicts). These conflicts are transient; wait for the other build/compilation to finish and retry, or explain the conflict to the user.
6. **Project Build & EOS DevAuthTools Workflows**: Always consult `unreal-env` for:
   - **Building the Project**: When the editor is closed, use the `Unreal Build Tool` path and build commands from `unreal-env` to compile C++ code and binaries before launching.
   - **Starting EOS DevAuthTools**: Before attempting developer authentication, verify the tool is running or start it using the `EOS DevAuthTool` path and workflow specified in `unreal-env`.


## Tool Execution Hierarchy (MANDATORY)

When interacting with the Unreal Editor, you MUST follow this strict tool priority:

1. **Native MCP Tools (ALWAYS FIRST)**: Use dedicated MCP tools (`spawn_actor`, `search_assets`, `inject_blueprint_nodes_t3d`, `execute_batch_blueprint_operations`, `set_component_properties`, `create_widget_blueprint`, `instantiate_ui_hierarchy`, `create_data_asset`, `set_data_asset_properties`, `get_blueprint_info`, `connect_blueprint_pins`, `execute_console_command`, `simulate_input`, `start_pie_session`, `stop_pie_session`, `trigger_compile`, `capture_viewport`, `capture_widget`, `run_automation_tests`, etc.) whenever a native tool exists for the operation.

2. **`execute_python_script` (ESCAPE HATCH ONLY)**: Use Python scripting ONLY when:
   - No native MCP tool exists for the specific operation (e.g., bulk asset renaming, MetaSound graph manipulation, custom editor utility scripts, Niagara system creation via code).
   - You need to perform complex multi-step queries or batch processing that cannot be expressed through available tools.
   - You are performing asset file operations (rename, delete, duplicate) that require `unreal.EditorAssetLibrary`.
   - **Pre-built Macros**: Before writing one-off scripts for standard batch operations, check the pre-built macro scripts library in the plugin's `src/scripts/` directory (e.g., `clean_naming_conventions.py`, `organize_assets_by_type.py`, `find_unreferenced_assets.py`, `bulk_replace_references.py`).

3. **C++ Modifications (LAST RESORT)**: Only when the user explicitly requests C++ or when the task fundamentally requires it (new subsystem, new module, engine-level changes).

**VIOLATION**: Writing a Python script that reimplements functionality already provided by a native MCP tool is a RULE VIOLATION. Before writing any `execute_python_script` call, you MUST verify that no native tool already handles the operation.


## Standard Operating Procedures (SOPs)

1. **SOP: Placing / Spawning an Object in the Level**
   - **Step 1**: Identify the asset class you want to spawn (e.g., StaticMeshComponent, a specific Blueprint).
   - **Step 2**: Use `search_assets` with a `class_filter` to find the exact path. Do NOT use `list_directory`.
   - **Step 3**: Use the native `spawn_actor` tool to place the object in the world.
   - **Step 4**: DO NOT attempt to write Python scripts or use console commands to place objects in the level.

2. **SOP: Modifying a Blueprint Graph**
   - **Step 1**: Use `get_blueprint_info` with `exclude_visual_layout=true` or `query_mode="interface_only"` to fetch the current graph interface without coordinates, saving massive tokens. Cache the returned `client_hash` and pass it in subsequent calls to return lightweight `up_to_date=true` results if unchanged.
   - **Step 2**: Use `execute_batch_blueprint_operations` to batch multiple graph modifications (components, variables, functions) in a single transaction, compiling once at the end.
   - **Step 3**: Draft the T3D nodes. To avoid node overlaps, you **MUST** call the `format_t3d_layout` tool on the drafted T3D text to calculate clean coordinates. Then, use `inject_blueprint_nodes_t3d` with the formatted T3D text and the inline `connections` array parameter to import nodes and wire them to existing nodes in a single tool call.
     - **CRITICAL**: When generating T3D (plain-text) representation for nodes (for `inject_blueprint_nodes_t3d` or copy-pasting), you **MUST** include a unique, valid `NodeGuid` parameter (a 32-character uppercase hexadecimal string, e.g., `NodeGuid=3E2A5D8446B84A29B52C2D812A2BD5F5`) for every single node. If omitted or duplicated, the imported nodes will lack a unique GUID, causing "missing NodeGuid" warnings during cooking.
   - **Step 4**: If isolated wiring is needed later, use `connect_blueprint_pins`. Compile step runs automatically on modifications.

3. **SOP: Implementation Priority (Native Tools > Blueprint > Python > C++)**
   - Follow the Tool Execution Hierarchy above. For gameplay logic implementation:
     1. Use native MCP tools for any supported operation.
     2. Use Blueprint manipulation (`inject_blueprint_nodes_t3d`, `execute_batch_blueprint_operations`) for gameplay logic that belongs in Blueprints.
     3. Use `execute_python_script` only as an escape hatch for editor utility tasks with no native tool equivalent.
     4. Modify C++ only when the user explicitly requests it or the task fundamentally requires engine-level code.

4. **SOP: Asset File Management (Moving, Renaming, Deleting)**
   - **CRITICAL**: Never use standard terminal shell commands (`mv`, `rm`, `del`) to move or rename Unreal Engine assets (.uasset/.umap) on disk. This will corrupt the project's internal asset registry.
   - **Step 1**: Before writing custom one-off scripts for standard batch operations, check the pre-built macro scripts library in the plugin's `src/scripts/` directory (e.g., `clean_naming_conventions.py`, `organize_assets_by_type.py`, `find_unreferenced_assets.py`, or `bulk_replace_references.py`).
   - **Step 2**: If no pre-built macro script exists for the specific task, write and run a custom Python script via `execute_python_script` utilizing `unreal.EditorAssetLibrary` functions (`rename_asset`, `delete_asset`, `duplicate_asset`) to handle asset operations safely.

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
   - **Step 1**: Use `create_widget_blueprint` to create the asset, then use `instantiate_ui_hierarchy` to construct the entire widget tree hierarchy, properties, slots, fonts, brushes, and event bindings in a single transaction and single compilation.
   - **Step 2**: Call `capture_widget` with the widget's `asset_path`. The tool will return a Base64 image artifact directly in the chat.
   - **Step 3**: Visually analyze the Base64 image artifact. Check alignments, padding, and colors.
   - **Step 4**: Iteratively adjust properties using `instantiate_ui_hierarchy` or specific setters, re-capturing as necessary until the layout perfectly matches the desired design.


## Routing & Sub-Agent Workflow Rules

### Agent Lite Routing (MANDATORY)
To conserve tokens and maintain task velocity, the Main Agent must route tasks efficiently:
- **Direct Thread Execution (Lite):** For simple read-only queries (e.g., "how does this Blueprint work?"), simple value edits (e.g., `set_node_pin_default`), or variable renaming, the Main Agent **MUST** run the task directly without spawning sub-agents.
- **Spawning Sub-agents (Telos Pipeline):** Only delegate to the sub-agent pipeline when the task involves multi-file structural changes, new gameplay logic systems, complex math graph building, or multi-step Blueprint asset creation.

### Sub-Agent Workflow for Blueprint Authoring (Antigravity 4-Layer Context)
When complex authoring is required, the Main Agent coordinates three specialized sub-agents:

1. **Planner Sub-agent (Role: `Blueprint Architect`)**
   - **Responsibility**: Decomposes the user's high-level goal into step-by-step T3D graph injection actions.
   - **Tools Allowed**: `search_similar_blueprints` (Layer 4 context), `get_blueprint_schema` (Layer 2), `search_assets`, read-only queries.
   - **MANDATORY PRE-FLIGHT GAP ANALYSIS:**
     - Before producing any plan, the Architect **MUST** call `get_blueprint_info` on the target Blueprint and fetch relevant Blueprint schema.
     - The Architect **MUST** generate a `<GAP_ANALYSIS>` block explicitly comparing the existing nodes, variables, and pins with the user's requested goal.
     - The Architect must detail what nodes are missing, which pins must be wired, and how the changes map to the target graph.

2. **Executor Sub-agent (Role: `Blueprint Engineer`)**
   - **Responsibility**: Translates the plan into actual `inject_blueprint_nodes_t3d` or `execute_batch_blueprint_operations` commands.
   - **Tools Allowed**: Modifying tools (Layer 1 manipulation), `format_t3d_layout`.
   - **Workflow**: 
     1. Consolidates edits and drafts the raw T3D node text.
     2. **MANDATORY BEAUTIFICATION PASS:** To prevent overlapping nodes, the Engineer **MUST** call the `format_t3d_layout` tool on the drafted T3D text *before* injection.
     3. Uses the formatted T3D with `execute_batch_blueprint_operations` or `inject_blueprint_nodes_t3d` to run in a single transaction.
     4. Audits pin states and reports completion.

3. **Verification Sub-agent (Role: `QA Auditor`)**
   - **Responsibility**: Checks the work after the transaction.
   - **Tools Allowed**: `compile_blueprint`, `run_automation_tests`.
   - **Workflow**: Compiles the Blueprint. If it fails, instructs the Executor to fix it via another transaction or explicitly undo it. If successful, completes the task.

**Routing Protocol**: The Main Agent MUST NOT execute complex Blueprint logic directly. It must delegate to the Architect, pass the plan to the Engineer, and then have the QA Auditor verify and commit.
