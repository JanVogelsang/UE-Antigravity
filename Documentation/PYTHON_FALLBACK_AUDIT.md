# Phase 1: Python Fallback & Native C++ Action Route Audit Report

## Document Information
- **Project**: UE-AgentFramework (`c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity`)
- **Target Output**: `Documentation/PYTHON_FALLBACK_AUDIT.md`
- **Milestone**: Phase 1 Audit Handoff & Synthesis
- **Date**: July 26, 2026
- **Status**: Complete Comprehensive Audit

---

## Executive Summary & Audit Matrix

### 1. Audit Scope
The Phase 1 audit comprehensively evaluated the entire `UE-AgentFramework` ecosystem to identify all instances where AI agent workflows, test suites, developer tools, or skills depend on Python script execution (`execute_python_script` or `unreal.*` module calls). The audit scope encompassed four primary pillars:

1. **Agent Skills (`UnrealEngine/skills/` & `.agents/skills/`)**: All 14 standard skill instruction sets used by AI agents.
2. **Automated Test Suite (`Tests/`)**: End-to-end integration and unit tests, specifically `test_e2e_integration.py`.
3. **Developer Utility Scripts (`UnrealEngine/src/scripts/`)**: Standalone developer automation scripts (`bulk_replace_references.py`, `clean_naming_conventions.py`, `find_unreferenced_assets.py`, `organize_assets_by_type.py`).
4. **Native C++ Action Executor Modules (`AgentFrameworkActions/`)**: Inventory of 183 implemented tools across 27 action modules in `AgentFramework/Source/AgentFrameworkActions/`.

---

### 2. Native C++ Action Inventory Summary

The in-editor C++ plugin `AgentFrameworkActions` hosts **27 action module directories** with **28 executor classes** implementing `IAgentFrameworkActionExecutor`. Centrally registered in `FAgentFrameworkHttpServer::RegisterAllExecutors` (`AgentFrameworkHttpServer.cpp`), these classes provide **183 discrete native tool routes** operating on port `18777`.

#### Complete Inventory by Action Module (183 Tools Across 27 Modules)

| # | Action Module | Executor Class | Tools Count | Tool Names Inventory |
|---|---|---|---|---|
| 1 | `AIAssistant` | `FAgentFrameworkAIAssistantActions` | 1 | `query_epic_assistant` |
| 2 | `Animation` | `FAgentFrameworkAnimationActions` | 13 | `create_anim_blueprint`, `import_animation_fbx`, `assign_anim_blueprint`, `create_anim_montage`, `get_anim_info`, `configure_motion_matching`, `create_ik_rig`, `create_ik_retargeter`, `create_control_rig`, `setup_motion_warping`, `create_blend_space`, `configure_anim_montage`, `map_live_link_source` |
| 3 | `BehaviorTree` | `FAgentFrameworkBehaviorTreeActions` | 10 | `create_blackboard`, `create_behavior_tree`, `inject_bt_nodes`, `configure_navmesh`, `create_state_tree`, `setup_mass_spawner`, `configure_mass_trait`, `setup_mass_crowd`, `query_smart_objects`, `run_eqs` |
| 4 | `Blueprint` | `FAgentFrameworkBlueprintActions` | 21 | `create_blueprint_actor`, `add_blueprint_component`, `add_blueprint_variable`, `add_blueprint_function`, `add_blueprint_event`, `compile_blueprint`, `set_blueprint_defaults`, `set_component_properties`, `inject_blueprint_nodes_t3d`, `get_blueprint_info`, `connect_blueprint_pins`, `add_enhanced_input_node`, `modify_blueprint`, `verify_blueprint_connections`, `set_node_pin_default`, `delete_blueprint_nodes`, `analyze_blueprint_graph`, `execute_batch_blueprint_operations`, `get_blueprint_schema`, `export_blueprint_summary`, `check_asset_state` |
| 5 | `Build` | `FAgentFrameworkBuildActions` | 2 | `build_lighting`, `package_project` |
| 6 | `Context` | `FAgentFrameworkContextActions` & `FAgentFrameworkDiscoveryActions` | 6 | `search_assets`, `list_directory`, `read_file_snippet`, `activate_skill`, `get_tool_info`, `list_tools_in_category` |
| 7 | `Cpp` | `FAgentFrameworkCppActions` | 6 | `create_cpp_class`, `modify_cpp_file`, `trigger_compile`, `regenerate_project_files`, `macro_create_cpp_class`, `get_cpp_reflection_info` |
| 8 | `DataAsset` | `FAgentFrameworkDataAssetActions` | 3 | `create_data_asset`, `set_data_asset_properties`, `get_data_asset_info` |
| 9 | `DataTable` | `FAgentFrameworkDataTableActions` | 2 | `create_data_table`, `import_json_to_datatable` |
| 10 | `Diagnostics` | `FAgentFrameworkDiagnosticsActions` | 2 | `read_message_log`, `shutdown_editor` |
| 11 | `GAS` | `FAgentFrameworkGASActions` | 5 | `gas_register_tags`, `gas_create_attribute_set`, `gas_setup_asc`, `gas_create_effect`, `gas_create_ability` |
| 12 | `Input` | `FAgentFrameworkInputActions` | 3 | `create_input_action`, `create_input_mapping_context`, `add_input_mapping` |
| 13 | `Level` | `FAgentFrameworkLevelActions` | 13 | `spawn_actor`, `place_light`, `modify_world_settings`, `configure_world_partition`, `create_foliage_type`, `paint_foliage_brush`, `create_landscape`, `create_landscape_grass_type`, `create_level_instance`, `create_packed_level_actor`, `setup_cine_camera_rig_rail`, `setup_dmx_patch`, `setup_chaos_vehicle` |
| 14 | `Material` | `FAgentFrameworkMaterialActions` | 5 | `create_material`, `create_material_instance`, `add_material_expression`, `connect_material_property`, `capture_material` |
| 15 | `Media` | `FAgentFrameworkMediaActions` | 5 | `create_media_player`, `create_media_texture`, `create_file_media_source`, `configure_media_player`, `get_media_info` |
| 16 | `Mesh` | `FAgentFrameworkMeshActions` | 10 | `import_mesh`, `import_assets_batch`, `configure_static_mesh`, `create_dynamic_mesh`, `audit_nanite_settings`, `setup_runtime_virtual_texture`, `setup_chaos_physics`, `setup_dataflow_graph`, `setup_clothing_simulation`, `setup_sparse_volume_texture` |
| 17 | `Niagara` | `FAgentFrameworkNiagaraActions` | 6 | `create_niagara_system`, `add_niagara_emitter`, `add_niagara_module`, `set_niagara_module_pin`, `compile_niagara_system`, `capture_niagara_system_isolated` |
| 18 | `PCG` | `FAgentFrameworkPCGActions` | 6 | `create_pcg_graph`, `attach_pcg_component`, `set_pcg_parameter`, `generate_pcg_local`, `get_pcg_info`, `wire_pcg_nodes` |
| 19 | `PIE` | `FAgentFrameworkPIEActions` | 6 | `start_pie_session`, `simulate_input`, `stop_pie_session`, `extract_ui_state`, `trigger_ui_element`, `query_world_state` |
| 20 | `Performance` | `FAgentFrameworkPerformanceActions` | 17 | `get_memory_stats`, `get_performance_stats`, `run_stat_command`, `analyze_asset_sizes`, `get_cvar`, `set_cvar`, `discover_cvars`, `execute_console_command`, `start_csv_profiler`, `stop_csv_profiler`, `read_profiling_file`, `get_scalability_settings`, `set_scalability_settings`, `get_renderer_settings`, `set_renderer_setting`, `adjust_lumen_settings`, `configure_hlod_setup` |
| 21 | `Python` | `FAgentFrameworkPythonActions` | 1 | `execute_python_script` (Python Escape Hatch) |
| 22 | `Sequencer` | `FAgentFrameworkSequencerActions` | 4 | `create_level_sequence`, `add_sequencer_track`, `add_sequencer_keyframe`, `configure_movie_render_job` |
| 23 | `Settings` | `FAgentFrameworkSettingsActions` | 6 | `read_config_value`, `write_config_value`, `macro_ensure_project_prerequisites`, `get_plugin_settings`, `list_config_sections`, `read_config_section` |
| 24 | `SourceControl` | `FAgentFrameworkSourceControlActions` | 8 | `source_control_checkout`, `source_control_add`, `source_control_revert`, `source_control_status`, `source_control_checkin`, `source_control_sync`, `source_control_history`, `source_control_diff` |
| 25 | `Validation` | `FAgentFrameworkValidationActions` | 5 | `validate_assets`, `run_automation_tests`, `validate_naming_conventions`, `validate_redirectors`, `validate_map` |
| 26 | `Viewport` | `FAgentFrameworkViewportActions` | 5 | `capture_viewport`, `set_viewport_camera`, `set_viewport_view_mode`, `set_viewport_realtime`, `focus_viewport_on_selection` |
| 27 | `Widget` | `FAgentFrameworkWidgetActions` | 16 | `create_widget_blueprint`, `add_widget`, `set_widget_slot`, `set_widget_property`, `set_widget_font`, `set_widget_brush`, `bind_widget_event`, `remove_widget`, `get_widget_tree`, `compile_widget_blueprint`, `macro_create_basic_ui_menu`, `capture_widget`, `instantiate_ui_hierarchy`, `get_widget_info`, `clear_panel_children`, `get_widget_slots` |

---

### 3. Master Audit Summary Table

The following master table details every identified Python fallback or C++ capability gap across all domain areas evaluated during Phase 1:

| # | Feature / Capability Gap | Origin Domain | Specific Source Path | Python API / Method Used | Proposed Native C++ Action Tool | Action Module |
|---|---|---|---|---|---|---|
| 1 | Blueprint Pin Disconnection | C++ Engine Gap | `AgentFrameworkActions/Blueprint` | N/A (Re-injects full T3D graph) | `disconnect_blueprint_pins` | `Blueprint` |
| 2 | Design-Time Sub-Object Mutation | Skill Fallback | `blueprint-authoring/SKILL.md` | `unreal.load_object` (colon paths) | `modify_blueprint_subobject` | `Blueprint` |
| 3 | Actor Replication Defaults | C++ Engine Gap | `AgentFrameworkActions/Blueprint` | N/A (Manual C++ editing) | `configure_actor_replication` | `Blueprint` |
| 4 | Variable Replication & RepNotify | C++ Engine Gap | `AgentFrameworkActions/Blueprint` | N/A (Manual C++ editing) | `set_variable_replication` | `Blueprint` |
| 5 | Enhanced Input Modifiers & Triggers | C++ Engine Gap | `AgentFrameworkActions/Input` | `unreal.InputMappingContext` scripts | `configure_input_mapping_modifiers_triggers` | `Input` |
| 6 | Niagara User Parameters & Curves | C++ Engine Gap | `AgentFrameworkActions/Niagara` | `unreal.NiagaraSystem` scripts | `set_niagara_parameter` | `Niagara` |
| 7 | PBR Material Auto-Wiring | Tool Churn Gap | `generate-assets/SKILL.md` | 6 sequential tool calls / script | `create_pbr_material_from_textures` | `Material` |
| 8 | Sound Wave & Cue Configuration | C++ Engine Gap | `AgentFrameworkActions/Media` | N/A (Manual asset setup) | `configure_sound_wave_cue` | `Media` |
| 9 | MetaSound Source Creation | C++ Engine Gap | `AgentFrameworkActions/MetaSound` | `IMetasoundFrontendDocumentBuilder` | `create_metasound_source` | `MetaSound` (New) |
| 10 | MetaSound Graph Node Wiring | C++ Engine Gap | `AgentFrameworkActions/MetaSound` | `IMetasoundFrontendDocumentBuilder` | `wire_metasound_nodes` | `MetaSound` (New) |
| 11 | Asset Reference Consolidation | Developer Script | `src/scripts/bulk_replace_references.py` | `unreal.EditorAssetLibrary.consolidate_assets` | `consolidate_asset_references` | `Context` / `Asset` |
| 12 | Batch Naming Standard Enforcement | Developer Script | `src/scripts/clean_naming_conventions.py` | `unreal.EditorAssetLibrary.list_assets` / `rename_asset` | `enforce_naming_conventions` | `Context` |
| 13 | Unreferenced Asset Auditing | Developer Script | `src/scripts/find_unreferenced_assets.py` | `unreal.AssetRegistryHelpers.get_asset_registry` | `find_unreferenced_assets` | `Diagnostics` |
| 14 | Asset Restructuring by Class | Developer Script | `src/scripts/organize_assets_by_type.py` | `unreal.EditorAssetLibrary.find_asset_data` | `organize_assets_by_type` | `Context` |
| 15 | Live UObject Property Reflection | Test Suite Gap | `Tests/test_e2e_integration.py` | `execute_python_script` | `inspect_uobject_properties` | `Diagnostics` |
| 16 | UMG Sub-widget Slot Properties | Skill Fallback | `blueprint-authoring/SKILL.md` | `unreal.load_object` (`slot.set_anchors`) | `set_widget_slot_properties` | `Widget` |
| 17 | Runtime PIE Widget Delegate Invocation | Skill Fallback | `unreal-testing-sops/SKILL.md` | `unreal.WidgetBlueprintLibrary` (`on_clicked.broadcast`) | `invoke_pie_widget_delegate` | `PIE` |
| 18 | Design-Time SCS Component Attachment | Skill Gap | `add-component/SKILL.md` | C++ constructor boilerplate only | `add_blueprint_component` | `Blueprint` |
| 19 | Runtime Active Widget Enumeration | Skill / Test Gap | `unreal-testing-sops/SKILL.md` | `unreal.WidgetBlueprintLibrary.get_all_widgets_of_class` | `get_active_runtime_widgets` | `Widget` / `PIE` |

---

## Section 1: Skills Audit (`UnrealEngine/skills/`)

### 1.1 Status Table for All 14 Skills

A line-by-line audit of all **14 skill folders** in `UnrealEngine/skills/` and `.agents/skills/` was conducted to determine their reliance on Python scripts (`execute_python_script` or `unreal.*` modules).

| # | Skill Name | Path / Location | Python Fallback Present? | Primary Purpose & Native Tool Alignment |
|---|---|---|---|---|
| 1 | `add-component` | `UnrealEngine/skills/add-component/SKILL.md` | ❌ No (C++ Code Pattern) | Adds components via C++ class code patterns; lacks design-time SCS blueprint tool call |
| 2 | `blueprint-authoring` | `UnrealEngine/skills/blueprint-authoring/SKILL.md` | ⚠️ **YES** (`unreal.load_object`) | Authors Blueprints & T3D nodes; falls back to Python for colon-path sub-object access |
| 3 | `create-actor` | `UnrealEngine/skills/create-actor/SKILL.md` | ❌ No | Generates C++ Actor boilerplate and optional Blueprint child via native tools |
| 4 | `create-interface` | `UnrealEngine/skills/create-interface/SKILL.md` | ❌ No | Generates C++ backing and Blueprint interface assets via native tools |
| 5 | `generate-assets` | `UnrealEngine/skills/generate-assets/SKILL.md` | ℹ️ CLI Python Script (External) | External REST API generation (Meshy/ElevenLabs); material setup requires tool churn |
| 6 | `niagara-authoring` | `UnrealEngine/skills/niagara-authoring/SKILL.md` | ❌ No | Authors Niagara particle systems using 6 native C++ Niagara tools |
| 7 | `pie-verifier` | `UnrealEngine/skills/pie-verifier/SKILL.md` | ❌ No | Runs PIE sessions and Slate UI verification via native C++ PIE tools |
| 8 | `python-env` | `UnrealEngine/skills/python-env/SKILL.md` | ℹ️ Environment Setup Only | Establishes virtualenv, pytest runner, and MCP stdio IPC configuration |
| 9 | `setup-input` | `UnrealEngine/skills/setup-input/SKILL.md` | ❌ No | Configures Enhanced Input actions via native Input tools |
| 10 | `setup-replication` | `UnrealEngine/skills/setup-replication/SKILL.md` | ❌ No | Documents network replication C++ code patterns; lacks direct asset tool routes |
| 11 | `unreal-instructions` | `UnrealEngine/skills/unreal-instructions/SKILL.md` | ❌ No | Mandatory entry point detailing Dual-MCP architecture and routing rules |
| 12 | `unreal-setup` | `UnrealEngine/skills/unreal-setup/SKILL.md` | ℹ️ CLI Pip Setup Only | Scans workspace, installs python dependencies, generates project index |
| 13 | `unreal-testing-sops` | `UnrealEngine/skills/unreal-testing-sops/SKILL.md` | ⚠️ **YES** (`unreal.WidgetBlueprintLibrary`) | Standard Operating Procedures for PIE; falls back to Python for UMG widget inspection |
| 14 | `project-index` | `.agents/skills/project-index/SKILL.md` | ❌ No | Dynamic project architecture map and module index for target project |

---

### 1.2 Detailed Analysis of Explicit Python Fallbacks in Skills

#### Fallback 1: `blueprint-authoring` (`unreal.load_object`)
* **Location**: `UnrealEngine/skills/blueprint-authoring/SKILL.md` (Lines 42-52)
* **Code Snippet**:
  ```python
  import unreal
  # Load internal sub-object using colon notation
  widget_obj = unreal.load_object(None, '/Game/UI/Path/W_MyWidget.W_MyWidget:WidgetTree.SubWidgetName')
  if widget_obj:
      slot = widget_obj.slot  # Access layout slot (e.g. CanvasPanelSlot)
      slot.set_z_order(-1)
      slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0), maximum=unreal.Vector2D(1,1)))
  ```
* **Analysis**: Standard Blueprint reflection tools (`set_blueprint_defaults`) only operate on the top-level Class Default Object (CDO). Nested sub-objects—such as child widgets inside a `WidgetTree` or sub-components added via the Simple Construction Script (SCS)—are inaccessible via standard property paths. Agents are forced to call `execute_python_script` with `unreal.load_object` to load sub-objects directly by path (`Asset.Asset:SubObjectPath`).

#### Fallback 2: `unreal-testing-sops` (`unreal.WidgetBlueprintLibrary`)
* **Location**: `UnrealEngine/skills/unreal-testing-sops/SKILL.md` (Lines 95-109)
* **Code Snippet**:
  ```python
  import unreal
  editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
  game_world = editor_subsystem.get_game_world()

  widgets = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(game_world, unreal.UserWidget, True)
  for w in widgets:
      if w.get_name() == "W_TauMainMenu_C" or "MainMenu" in w.get_name():
          pvp_button = w.get_editor_property("PvPButton")
          if pvp_button:
              pvp_button.on_clicked.broadcast()
              print("PvP button clicked programmatically")
              break
  ```
* **Analysis**: Existing native C++ tools (`extract_ui_state`, `trigger_ui_element`) rely on Slate geometry matching and synthetic mouse click simulation. If a UI element is collapsed, off-screen, or un-focused, physical Slate click simulation fails. Testing SOPs fall back to Python to query runtime `UUserWidget` instances directly from the PIE `UWorld` and trigger multicast delegates (`OnClicked.Broadcast()`) programmatically.

---

### 1.3 Tool Gap Analysis for Skills

#### Gap 1: `add-component` (SCS Component Attachment)
* **Context**: `add-component/SKILL.md` provides C++ code patterns for constructor component attachment (`CreateDefaultSubobject`).
* **Tool Gap**: When an agent needs to attach a new component to an existing Blueprint asset (`.uasset`) at design time, no native action route exists. `execute_batch_blueprint_operations` operates on Blueprint graph nodes, but cannot manipulate the `USimpleConstructionScript` (SCS) node tree. The proposed `add_blueprint_component` native action closes this gap.

#### Gap 2: `generate-assets` (PBR Material Creation Tool Churn)
* **Context**: `generate-assets/SKILL.md` imports 3D meshes and PBR texture maps generated via external AI services (Meshy, ElevenLabs).
* **Tool Gap**: Constructing a PBR material for an imported mesh currently requires **6 sequential tool calls** (`create_material`, `add_material_expression` x4, `connect_material_property` x4, `assign_material`). This multi-step process introduces high token overhead and failure risks. The proposed `create_pbr_material_from_textures` C++ action automates texture sample node instantiation, PBR pin connection (`BaseColor`, `Normal`, `Roughness`, `Metallic`, `AO`), and mesh material assignment in a single atomic operation.

---

## Section 2: Tests & Infrastructure Audit (`Tests/`, `UnrealEngine/src/scripts/`)

### 2.1 Test Suite Usage (`Tests/test_e2e_integration.py`)

The automated E2E integration test suite was audited to evaluate reliance on Python script fallbacks:

* **File**: `Tests/test_e2e_integration.py`
* **Test Function**: `test_cpp_mcp_execute_python_script_validation`
* **Current Python Fallback Code**:
  ```python
  response = mock_agent_client.call_cpp_tool(
      "execute_python_script",
      {
          "script": "print('hello')",
          "justification_why_native_tools_or_skills_are_insufficient": "We need to run custom Python reflection because no native tool can read metadata of non-blueprint UObjects"
      }
  )
  ```
* **Root Cause**: The test explicitly states that Python execution is required because `FAgentFrameworkBlueprintActions::get_blueprint_schema` only handles `UBlueprint` assets, and `get_cpp_reflection_info` inspects C++ header declarations. No native tool exists to serialize live property values, struct members, and metadata of arbitrary non-Blueprint `UObject` instances (e.g. `UDataAsset`, `USoundBase`, `UWorld` sub-objects).
* **Resolution**: Implementing `inspect_uobject_properties` in `FAgentFrameworkDiagnosticsActions` allows live `UObject` property reflection without Python.

---

### 2.2 Developer Utility Scripts Analysis (`UnrealEngine/src/scripts/`)

All four developer utility scripts in `UnrealEngine/src/scripts/` rely entirely on the Python `unreal.*` Editor module. Each script represents a capability gap in `AgentFrameworkActions`:

#### 1. `bulk_replace_references.py`
* **Python API Used**: `unreal.EditorAssetLibrary.consolidate_assets(target_asset, [source_asset])`
* **Capability Gap**: Neither `FAgentFrameworkContextActions` nor `FAgentFrameworkLevelActions` provides reference replacement or object consolidation. Merging duplicate materials or replacing asset references requires calling `ObjectTools::ConsolidateObjects` or `UEditorAssetLibrary::ConsolidateAssets`.
* **Native C++ Action Solution**: `consolidate_asset_references` in `FAgentFrameworkContextActions`.

#### 2. `clean_naming_conventions.py`
* **Python API Used**: `unreal.EditorAssetLibrary.list_assets()`, `unreal.EditorAssetLibrary.rename_asset()`
* **Capability Gap**: While single asset rename exists, there is no batch C++ action that scans content directories, evaluates class-to-prefix rules (`BP_`, `SM_`, `T_`, `M_`, `MI_`, `NS_`, `WBP_`), and enforces naming standards recursively.
* **Native C++ Action Solution**: `enforce_naming_conventions` in `FAgentFrameworkContextActions`.

#### 3. `find_unreferenced_assets.py`
* **Python API Used**: `unreal.AssetRegistryHelpers.get_asset_registry().get_referencers()`
* **Capability Gap**: No native C++ action queries the `IAssetRegistry` dependency graph to find unreferenced or orphan assets.
* **Native C++ Action Solution**: `find_unreferenced_assets` in `FAgentFrameworkDiagnosticsActions`.

#### 4. `organize_assets_by_type.py`
* **Python API Used**: `unreal.EditorAssetLibrary.list_assets()`, `unreal.EditorAssetLibrary.find_asset_data()`
* **Capability Gap**: Lacks an automated C++ route to sort mixed asset directories into type-specific target subfolders (`Blueprints/`, `Materials/`, `Meshes/`, `Audio/`, `Effects/`, `Maps/`, `UI/`).
* **Native C++ Action Solution**: `organize_assets_by_type` in `FAgentFrameworkContextActions`.

---

## Section 3: Native C++ Action Route Gaps in `AgentFrameworkActions`

A detailed architectural breakdown of the **10 granular C++ action gaps** in `AgentFrameworkActions`:

1. **Blueprint Graph Pin Disconnection**:
   `FAgentFrameworkBlueprintActions::connect_blueprint_pins` establishes pin links. However, no C++ tool exists to explicitly break pin links or disconnect specific pins on Blueprint nodes. Disconnecting a pin currently requires re-injecting full T3D graph blocks or invoking Python graph APIs.
2. **Sub-Object Property Mutation**:
   `set_blueprint_defaults` sets CDO root properties. Internal sub-objects (UMG `WidgetTree` children, SCS sub-components, or nested sub-objects) cannot be mutated via top-level reflection, forcing Python colon-path loading (`unreal.load_object(None, 'Path:SubObject')`).
3. **Actor Replication Defaults**:
   Configuring `bReplicates`, `bReplicateMovement`, `NetDormancy`, and update frequencies on Blueprint actors requires manual C++ edits or Python script execution.
4. **Variable Replication & RepNotify**:
   Setting variable replication modes (`Replicated` vs `RepNotify`), custom RepNotify callback function names, and replication conditions (`COND_OwnerOnly`, `COND_SkipOwner`) cannot be performed via tool calls.
5. **Enhanced Input Key Modifiers & Triggers**:
   `add_input_mapping` creates a basic key mapping but cannot attach key modifiers (`FEnhancedInputModifierNegate`, `FEnhancedInputModifierSwizzleAxis`) or triggers (`UInputTriggerPressed`, `UInputTriggerHold`, `UInputTriggerTap`) to key mappings.
6. **Niagara User Parameters & Dynamic Curves**:
   `set_niagara_module_pin` handles module-level input pins, but cannot write System/Emitter-level User parameters (`User.MyColor`, `User.SpawnRate`) or dynamic curve parameters (`UCurveFloat`, `UCurveLinearColor`) via `UNiagaraUserRedirectionParameterStore`.
7. **PBR Material Auto-Wiring**:
   Importing AI-generated models and PBR textures requires 5-6 sequential tool calls to create nodes and connect pins individually. An atomic C++ action `create_pbr_material_from_textures` builds the complete material graph instantly.
8. **Sound Wave & Sound Cue Configuration**:
   Importing audio files creates `USoundWave`, but setting looping/volume/pitch properties and creating `USoundCue` assets with attenuation settings requires manual asset setup or Python scripts.
9. **MetaSound Audio System Initialization**:
   `AgentFrameworkActions` completely lacks a MetaSound action executor module (0 tools). Creating MetaSound assets requires Python script invocation via `IMetasoundFrontendDocumentBuilder`.
10. **MetaSound Graph Node Wiring**:
    No native C++ route exists to inject MetaSound nodes (Oscillators, Envelopes, WavePlayers) and wire audio/trigger pins inside MetaSound graphs.

---

## Section 4: Phase 2 Native C++ Action API Specifications

This section provides complete, detailed specification entries for each proposed Native C++ Action API required to achieve 100% native C++ coverage in Phase 2.

---

### Specification 1: `disconnect_blueprint_pins`

* **Subsystem Name**: Blueprint Graph Node Wiring
* **Source / Context**: `AgentFrameworkActions/Blueprint` (`FAgentFrameworkBlueprintActions`)
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  # Re-injecting whole node T3D or calling unreal Python graph API
  node = unreal.BlueprintEditorLibrary.find_node_by_guid(bp, node_guid)
  pin = node.find_pin("TargetPin")
  pin.break_all_pin_links()
  ```
* **Reason Native Actions Are Insufficient**: `connect_blueprint_pins` exists, but no native route can break specific pin links or clear all connections on a node pin.
* **Proposed Action Specification**:
  * **Route Name**: `disconnect_blueprint_pins`
  * **Action Module**: `AgentFrameworkBlueprintActions` (`FAgentFrameworkBlueprintActions`)
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "TargetAsset": {
          "type": "string",
          "description": "Object path of the target Blueprint (e.g. '/Game/Blueprints/BP_Player')"
        },
        "NodeGuid": {
          "type": "string",
          "description": "32-character hex GUID of the target node"
        },
        "PinName": {
          "type": "string",
          "description": "Name of the pin to disconnect (e.g. 'Execute', 'Target', 'Output')"
        },
        "TargetNodeGuid": {
          "type": "string",
          "description": "Optional: Specific target node GUID to break connection to"
        },
        "TargetPinName": {
          "type": "string",
          "description": "Optional: Specific target pin name to break connection to"
        },
        "bDisconnectAll": {
          "type": "boolean",
          "default": false,
          "description": "If true, breaks all links on the specified pin"
        }
      },
      "required": ["TargetAsset", "NodeGuid", "PinName"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "TargetAsset": { "type": "string" },
        "NodeGuid": { "type": "string" },
        "PinName": { "type": "string" },
        "DisconnectedLinksCount": { "type": "integer" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "DisconnectedLinksCount", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load `UBlueprint` asset via `StaticLoadObject`.
    2. Find `UEdGraphNode` matching `NodeGuid` across all graph instances (`BP->UbergraphPages`, `FunctionGraphs`).
    3. Locate `UEdGraphPin` matching `PinName`.
    4. Call `Pin->BreakLinkTo(TargetPin)` or `Pin->BreakAllPinLinks()`.
    5. Trigger `FBlueprintEditorUtils::MarkBlueprintAsModified(BP)` and recompile.

---

### Specification 2: `modify_blueprint_subobject`

* **Subsystem Name**: Design-Time Sub-Object & UMG Child Property Mutation
* **Source / Context**: `blueprint-authoring` skill & `AgentFrameworkActions/Blueprint`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  widget_obj = unreal.load_object(None, '/Game/UI/W_MyWidget.W_MyWidget:WidgetTree.SubWidgetName')
  if widget_obj:
      widget_obj.slot.set_z_order(-1)
  ```
* **Reason Native Actions Are Insufficient**: `set_blueprint_defaults` only targets CDO root properties. Private or nested sub-objects (UMG `WidgetTree` children, SCS sub-components) cannot be loaded or mutated via standard property specifiers.
* **Proposed Action Specification**:
  * **Route Name**: `modify_blueprint_subobject`
  * **Action Module**: `AgentFrameworkBlueprintActions` / `AgentFrameworkWidgetActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "AssetPath": {
          "type": "string",
          "description": "Long package path to Blueprint asset (e.g. '/Game/UI/W_MainMenu')"
        },
        "SubObjectPath": {
          "type": "string",
          "description": "Relative sub-object path using colon/dot notation (e.g. 'WidgetTree.Btn_Start')"
        },
        "Properties": {
          "type": "object",
          "additionalProperties": true,
          "description": "Key-value map of property names to serialized values"
        }
      },
      "required": ["AssetPath", "SubObjectPath", "Properties"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "AssetPath": { "type": "string" },
        "SubObjectPath": { "type": "string" },
        "SubObjectClass": { "type": "string" },
        "ModifiedPropertiesCount": { "type": "integer" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "ModifiedPropertiesCount", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Construct full sub-object path: `FString FullPath = FString::Printf(TEXT("%s.%s:%s"), *AssetPath, *AssetName, *SubObjectPath);`
    2. Load sub-object using `StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath);`
    3. Iterate property key-value map, find `FProperty*` via `SubObj->GetClass()->FindPropertyByName(*PropName)`.
    4. Call `Prop->ImportText_Direct` to set values and mark asset modified via `FBlueprintEditorUtils::MarkBlueprintAsModified`.

---

### Specification 3: `configure_actor_replication`

* **Subsystem Name**: Actor Network Replication Defaults
* **Source / Context**: `setup-replication` skill & `AgentFrameworkActions/Blueprint`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  bp = unreal.load_object(None, '/Game/BP_Pawn')
  cdo = unreal.get_default_object(bp.generated_class)
  cdo.set_editor_property('bReplicates', True)
  cdo.set_editor_property('bReplicateMovement', True)
  ```
* **Reason Native Actions Are Insufficient**: No native tool exposes direct network replication default configuration (`bReplicates`, `bReplicateMovement`, `NetDormancy`, `NetUpdateFrequency`) on Blueprint CDOs.
* **Proposed Action Specification**:
  * **Route Name**: `configure_actor_replication`
  * **Action Module**: `AgentFrameworkBlueprintActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "TargetAsset": {
          "type": "string",
          "description": "Object path of the Blueprint actor (e.g. '/Game/Blueprints/BP_NetworkPawn')"
        },
        "bReplicates": { "type": "boolean", "default": true },
        "bReplicateMovement": { "type": "boolean", "default": true },
        "NetDormancy": {
          "type": "string",
          "enum": ["DORM_Never", "DORM_Awake", "DORM_DormantAll", "DORM_DormantPartial", "DORM_Initial"],
          "default": "DORM_Never"
        },
        "NetUpdateFrequency": { "type": "number", "default": 100.0 },
        "NetPriority": { "type": "number", "default": 1.0 }
      },
      "required": ["TargetAsset"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "TargetAsset": { "type": "string" },
        "bReplicates": { "type": "boolean" },
        "bReplicateMovement": { "type": "boolean" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "TargetAsset", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load `UBlueprint` asset.
    2. Access `AActor* CDO = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());`
    3. Modify `CDO->SetReplicates(bReplicates)`, `CDO->SetReplicateMovement(bReplicateMovement)`, `CDO->NetDormancy`, `CDO->NetUpdateFrequency`.
    4. Save CDO changes and recompile blueprint.

---

### Specification 4: `set_variable_replication`

* **Subsystem Name**: Blueprint Variable Replication & RepNotify
* **Source / Context**: `setup-replication` skill & `AgentFrameworkActions/Blueprint`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  # Modifying FBPVariableDescription via Python script
  bp = unreal.load_object(None, '/Game/BP_PlayerState')
  # Manual variable replication property injection
  ```
* **Reason Native Actions Are Insufficient**: `add_blueprint_variable` creates variables but cannot configure variable replication flags (`Replicated` vs `RepNotify`), custom RepNotify callback function names, or replication conditions.
* **Proposed Action Specification**:
  * **Route Name**: `set_variable_replication`
  * **Action Module**: `AgentFrameworkBlueprintActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "TargetAsset": { "type": "string", "description": "Object path of the Blueprint asset" },
        "VariableName": { "type": "string", "description": "Name of the target member variable" },
        "ReplicationType": {
          "type": "string",
          "enum": ["None", "Replicated", "RepNotify"],
          "default": "Replicated"
        },
        "RepNotifyFunc": {
          "type": "string",
          "description": "Optional: Custom RepNotify callback function name (e.g. 'OnRep_Health')"
        },
        "ReplicationCondition": {
          "type": "string",
          "enum": ["COND_None", "COND_InitialOnly", "COND_OwnerOnly", "COND_SkipOwner", "COND_SimulatedOnly", "COND_AutonomousOnly", "COND_Custom"],
          "default": "COND_None"
        }
      },
      "required": ["TargetAsset", "VariableName", "ReplicationType"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "TargetAsset": { "type": "string" },
        "VariableName": { "type": "string" },
        "ReplicationType": { "type": "string" },
        "RepNotifyFunc": { "type": "string" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "VariableName", "ReplicationType", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load `UBlueprint` asset, find `FBPVariableDescription* VarDesc` matching `VariableName` in `BP->NewVariables`.
    2. Set `VarDesc->ReplicationType` to `ELifetimeCondition` / `EPropertyReplicationType`.
    3. If `RepNotify` selected, invoke `FBlueprintEditorUtils::CreateUserDefinedFunction` to auto-generate `OnRep_VariableName` callback graph if it does not exist.
    4. Call `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP)`.

---

### Specification 5: `configure_input_mapping_modifiers_triggers`

* **Subsystem Name**: Enhanced Input Mapping Modifiers & Triggers
* **Source / Context**: `setup-input` skill & `AgentFrameworkActions/Input`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  imc = unreal.load_object(None, '/Game/Input/IMC_Default')
  mapping = imc.add_mapping(input_action, key)
  mod = unreal.EnhancedInputModifierNegate()
  mapping.modifiers.append(mod)
  ```
* **Reason Native Actions Are Insufficient**: `add_input_mapping` creates key mappings, but cannot attach key modifiers (`FEnhancedInputModifierNegate`, `FEnhancedInputModifierSwizzleAxis`) or triggers (`UInputTriggerPressed`, `UInputTriggerHold`).
* **Proposed Action Specification**:
  * **Route Name**: `configure_input_mapping_modifiers_triggers`
  * **Action Module**: `AgentFrameworkInputActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "ContextAsset": { "type": "string", "description": "Object path of UInputMappingContext asset" },
        "InputActionAsset": { "type": "string", "description": "Object path of target UInputAction asset" },
        "Key": { "type": "string", "description": "Key identifier (e.g. 'W', 'Gamepad_LeftStick_Y')" },
        "Modifiers": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "Type": { "type": "string", "enum": ["Negate", "SwizzleAxis", "Scalar", "DeadZone", "Smooth", "ResponseCurve"] },
              "Order": { "type": "string", "enum": ["YXZ", "ZYX", "XZY"], "default": "YXZ" },
              "ScalarVector": {
                "type": "object",
                "properties": { "X": {"type": "number"}, "Y": {"type": "number"}, "Z": {"type": "number"} }
              }
            },
            "required": ["Type"]
          }
        },
        "Triggers": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "Type": { "type": "string", "enum": ["Pressed", "Released", "Hold", "Tap", "Pulse", "ChordAction"] },
              "HoldTimeThreshold": { "type": "number", "default": 0.5 },
              "bIsOneShot": { "type": "boolean", "default": true }
            },
            "required": ["Type"]
          }
        }
      },
      "required": ["ContextAsset", "InputActionAsset", "Key"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "ContextAsset": { "type": "string" },
        "Key": { "type": "string" },
        "AppliedModifiersCount": { "type": "integer" },
        "AppliedTriggersCount": { "type": "integer" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "AppliedModifiersCount", "AppliedTriggersCount", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load `UInputMappingContext` and `UInputAction`.
    2. Find `FEnhancedActionKeyMapping*` matching `Key` and `InputAction`.
    3. Instantiate requested `UInputModifier` (e.g. `UInputModifierNegate`, `UInputModifierSwizzleAxis`) and `UInputTrigger` objects via `NewObject`.
    4. Append instances to `Mapping->Modifiers` and `Mapping->Triggers` arrays, then mark package dirty.

---

### Specification 6: `set_niagara_parameter`

* **Subsystem Name**: Niagara System User Parameters & Curves
* **Source / Context**: `niagara-authoring` skill & `AgentFrameworkActions/Niagara`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  system = unreal.load_object(None, '/Game/VFX/NS_Explosion')
  override_store = system.get_override_parameters()
  override_store.set_parameter_value_float('User.SpawnRate', 500.0)
  ```
* **Reason Native Actions Are Insufficient**: `set_niagara_module_pin` only mutates module-level inputs. Setting System/Emitter-level User parameters (`User.MyColor`, `User.SpawnRate`) or dynamic curve parameters (`UCurveFloat`, `UCurveLinearColor`) requires native parameter store access.
* **Proposed Action Specification**:
  * **Route Name**: `set_niagara_parameter`
  * **Action Module**: `AgentFrameworkNiagaraActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "SystemAsset": { "type": "string", "description": "Object path of UNiagaraSystem asset" },
        "ParameterScope": { "type": "string", "enum": ["User", "System", "Emitter"], "default": "User" },
        "ParameterName": { "type": "string", "description": "Name of parameter (e.g. 'SpawnRate', 'PrimaryColor')" },
        "DataType": {
          "type": "string",
          "enum": ["Float", "Vector2", "Vector3", "LinearColor", "Bool", "Int32", "CurveFloat", "CurveLinearColor"],
          "default": "Float"
        },
        "Value": { "description": "Constant scalar, vector, or color payload" },
        "CurveKeys": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": { "Time": { "type": "number" }, "Value": { "type": "number" } },
            "required": ["Time", "Value"]
          }
        }
      },
      "required": ["SystemAsset", "ParameterName", "DataType"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "SystemAsset": { "type": "string" },
        "ParameterName": { "type": "string" },
        "BoundDataType": { "type": "string" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "ParameterName", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load `UNiagaraSystem` asset.
    2. Access `FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();`
    3. Construct `FNiagaraVariable` with name `FString::Printf(TEXT("%s.%s"), *Scope, *ParamName)` and matching `FNiagaraTypeDefinition`.
    4. Set parameter data or instantiate `UCurveFloat` keyframes into parameter store, then request system recompile via `System->RequestCompile(false)`.

---

### Specification 7: `create_pbr_material_from_textures`

* **Subsystem Name**: Generative AI PBR Material Generation
* **Source / Context**: `generate-assets` skill & `AgentFrameworkActions/Material`
* **Current Python Fallback Snippet**:
  ```python
  # Multi-step tool churn in Python / REST scripts:
  mat = unreal.MaterialEditingLibrary.create_material(mat_path)
  ts_base = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionTextureSample)
  ts_base.texture = unreal.load_object(None, base_color_path)
  unreal.MaterialEditingLibrary.connect_material_property(ts_base, "", unreal.MaterialProperty.MP_BASE_COLOR)
  ```
* **Reason Native Actions Are Insufficient**: Creating a complete PBR material from imported textures currently requires 6 sequential tool calls. An atomic C++ action builds the complete material graph instantly.
* **Proposed Action Specification**:
  * **Route Name**: `create_pbr_material_from_textures`
  * **Action Module**: `AgentFrameworkMaterialActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "MaterialPath": { "type": "string", "description": "Destination package path for Material asset" },
        "BaseColorTexturePath": { "type": "string", "description": "Object path of BaseColor texture" },
        "NormalTexturePath": { "type": "string", "description": "Optional: Object path of Normal map texture" },
        "RoughnessTexturePath": { "type": "string", "description": "Optional: Object path of Roughness texture" },
        "MetallicTexturePath": { "type": "string", "description": "Optional: Object path of Metallic texture" },
        "AOTexturePath": { "type": "string", "description": "Optional: Object path of Ambient Occlusion texture" },
        "BlendMode": { "type": "string", "enum": ["Opaque", "Masked", "Translucent", "Additive"], "default": "Opaque" },
        "ShadingModel": { "type": "string", "enum": ["DefaultLit", "Unlit", "Subsurface", "ClearCoat"], "default": "DefaultLit" }
      },
      "required": ["MaterialPath", "BaseColorTexturePath"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "MaterialPath": { "type": "string" },
        "ExpressionsCreatedCount": { "type": "integer" },
        "ConnectionsWiredCount": { "type": "integer" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "MaterialPath", "ExpressionsCreatedCount", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Create `UMaterial` asset via `AssetToolsModule.Get().CreateAsset()`.
    2. Set `BlendMode` and `ShadingModel`.
    3. Instantiate `UMaterialExpressionTextureSample` nodes for provided texture paths using `UMaterialEditingLibrary::CreateMaterialExpression`.
    4. Wire texture sample expression outputs to material inputs (`MP_BaseColor`, `MP_Normal`, `MP_Roughness`, `MP_Metallic`, `MP_AmbientOcclusion`).
    5. Compile material via `UMaterialEditingLibrary::RecompileMaterial(Material)`.

---

### Specification 8: `configure_sound_wave_cue`

* **Subsystem Name**: Sound Wave & Sound Cue Asset Configuration
* **Source / Context**: `generate-assets` skill & `AgentFrameworkActions/Media`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  sw = unreal.load_object(None, '/Game/Audio/SW_Explosion')
  sw.looping = True
  sw.volume = 1.5
  factory = unreal.SoundCueFactoryNew()
  cue = unreal.AssetToolsHelpers.get_asset_tools().create_asset("SC_Explosion", "/Game/Audio", unreal.SoundCue, factory)
  ```
* **Reason Native Actions Are Insufficient**: Importing audio creates `USoundWave`, but setting looping/volume/pitch flags and creating `USoundCue` assets with attenuation settings requires manual C++ tools.
* **Proposed Action Specification**:
  * **Route Name**: `configure_sound_wave_cue`
  * **Action Module**: `AgentFrameworkMediaActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "SoundWaveAsset": { "type": "string", "description": "Object path of imported SoundWave asset" },
        "CueAssetPath": { "type": "string", "description": "Optional: Destination path for USoundCue asset" },
        "bLooping": { "type": "boolean", "default": false },
        "VolumeMultiplier": { "type": "number", "default": 1.0 },
        "PitchMultiplier": { "type": "number", "default": 1.0 },
        "AttenuationAssetPath": { "type": "string", "description": "Optional: Path to USoundAttenuation asset" }
      },
      "required": ["SoundWaveAsset"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "SoundWaveAsset": { "type": "string" },
        "CueAssetPath": { "type": "string" },
        "bLooping": { "type": "boolean" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "SoundWaveAsset", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load `USoundWave` asset, modify `bLooping`, `Volume`, `Pitch` properties.
    2. If `CueAssetPath` specified, instantiate `USoundCue` asset via `USoundCueFactoryNew`.
    3. Add `USoundNodeWavePlayer` node to cue graph, bind `USoundWave`, connect to `USoundCue::FirstNode`.
    4. Assign `USoundAttenuation` asset if provided.

---

### Specification 9: `create_metasound_source`

* **Subsystem Name**: MetaSound Audio Source Initialization
* **Source / Context**: `AgentFrameworkActions/MetaSound` (New Executor)
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  # Uses unreal Python MetasoundFrontend document builder bindings
  builder = unreal.MetaSoundFrontendDocumentBuilder()
  builder.create_preset(...)
  ```
* **Reason Native Actions Are Insufficient**: `AgentFrameworkActions` completely lacks a MetaSound action module (0 tools). Creating MetaSound assets requires Python script invocation via `IMetasoundFrontendDocumentBuilder`.
* **Proposed Action Specification**:
  * **Route Name**: `create_metasound_source`
  * **Action Module**: `AgentFrameworkMetaSoundActions` (New Module)
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "AssetPath": { "type": "string", "description": "Destination path for MetaSoundSource asset" },
        "OutputFormat": { "type": "string", "enum": ["Mono", "Stereo", "Quad", "5.1", "7.1"], "default": "Stereo" },
        "bIsPreset": { "type": "boolean", "default": false }
      },
      "required": ["AssetPath"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "AssetPath": { "type": "string" },
        "OutputFormat": { "type": "string" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "AssetPath", "OutputFormat", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Obtain `IMetaSoundEngineModule` and `Metasound::Frontend::FDocumentBuilder`.
    2. Create `UMetaSoundSource` asset via `FKismetEditorUtilities` or `FAssetToolsModule`.
    3. Initialize document builder, set output channel format (`Mono`/`Stereo`), and register root graph.

---

### Specification 10: `wire_metasound_nodes`

* **Subsystem Name**: MetaSound Audio Graph Wiring
* **Source / Context**: `AgentFrameworkActions/MetaSound` (New Executor)
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  builder = unreal.MetaSoundFrontendDocumentBuilder()
  node_id = builder.add_node_by_class_name("WavePlayer:Mono")
  builder.connect_nodes(node_id, "Audio", output_id, "Audio")
  ```
* **Reason Native Actions Are Insufficient**: No C++ tool exists to inject MetaSound nodes (Oscillators, Envelopes, WavePlayers) and wire audio/trigger pins inside MetaSound graphs natively.
* **Proposed Action Specification**:
  * **Route Name**: `wire_metasound_nodes`
  * **Action Module**: `AgentFrameworkMetaSoundActions` (New Module)
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "AssetPath": { "type": "string", "description": "Object path of target MetaSoundSource asset" },
        "NodesToAdd": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "NodeClassName": { "type": "string", "description": "e.g. 'WavePlayer:Mono', 'Sine:Audio', 'ADSR:Envelope'" },
              "NodeName": { "type": "string", "description": "Custom identifier for the node" }
            },
            "required": ["NodeClassName", "NodeName"]
          }
        },
        "ConnectionsToWire": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "FromNode": { "type": "string" }, "FromPin": { "type": "string" },
              "ToNode": { "type": "string" }, "ToPin": { "type": "string" }
            },
            "required": ["FromNode", "FromPin", "ToNode", "ToPin"]
          }
        }
      },
      "required": ["AssetPath"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "AssetPath": { "type": "string" },
        "NodesAddedCount": { "type": "integer" },
        "ConnectionsWiredCount": { "type": "integer" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "NodesAddedCount", "ConnectionsWiredCount", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load `UMetaSoundSource` asset and attach `Metasound::Frontend::FDocumentBuilder`.
    2. Add requested frontend nodes via `Builder.AddNode(NodeClassName)`.
    3. Connect specified node pins via `Builder.ConnectNodes(FromNodeHandle, FromPin, ToNodeHandle, ToPin)`.
    4. Save document builder changes and recompile MetaSound source.

---

### Specification 11: `consolidate_asset_references`

* **Subsystem Name**: Asset Management & Consolidation
* **Source / Context**: `UnrealEngine/src/scripts/bulk_replace_references.py`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  source_asset = unreal.EditorAssetLibrary.load_asset(source_path)
  target_asset = unreal.EditorAssetLibrary.load_asset(target_path)
  unreal.EditorAssetLibrary.consolidate_assets(target_asset, [source_asset])
  ```
* **Reason Native Actions Are Insufficient**: `FAgentFrameworkContextActions` lacks a route to perform object reference consolidation (`ObjectTools::ConsolidateObjects` or `UEditorAssetLibrary::ConsolidateAssets`).
* **Proposed Action Specification**:
  * **Route Name**: `consolidate_asset_references`
  * **Action Module**: `FAgentFrameworkContextActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "source_asset_path": { "type": "string", "description": "Package path of asset to replace and delete" },
        "target_asset_path": { "type": "string", "description": "Package path of asset to replace references with" }
      },
      "required": ["source_asset_path", "target_asset_path"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "source_asset_path": { "type": "string" },
        "target_asset_path": { "type": "string" },
        "ResultMessage": { "type": "string" },
        "Errors": { "type": "array", "items": { "type": "string" } }
      },
      "required": ["bSuccess", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load source and target `UObject` pointers using `StaticLoadObject`.
    2. Invoke `UEditorAssetLibrary::ConsolidateAssets(TargetAsset, { SourceAsset })` or `ObjectTools::ConsolidateObjects`.
    3. Return JSON response confirming reference consolidation.

---

### Specification 12: `enforce_naming_conventions`

* **Subsystem Name**: Asset Hygiene & Naming Conventions
* **Source / Context**: `UnrealEngine/src/scripts/clean_naming_conventions.py`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  assets = unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True)
  for asset_path in assets:
      asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
      # Check prefix and rename asset
  ```
* **Reason Native Actions Are Insufficient**: Single rename exists, but batch evaluation of UE5 asset class prefixes (`BP_`, `SM_`, `T_`, `M_`, `MI_`, `NS_`, `WBP_`) across directory trees is missing.
* **Proposed Action Specification**:
  * **Route Name**: `enforce_naming_conventions`
  * **Action Module**: `FAgentFrameworkContextActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "folder_path": { "type": "string", "description": "Package directory path to scan" },
        "recursive": { "type": "boolean", "default": true },
        "dry_run": { "type": "boolean", "default": false }
      },
      "required": ["folder_path"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "folder_path": { "type": "string" },
        "renamed_assets_count": { "type": "integer" },
        "renamed_details": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": { "old_path": { "type": "string" }, "new_path": { "type": "string" } }
          }
        },
        "ResultMessage": { "type": "string" }
      },
      "required": ["bSuccess", "renamed_assets_count", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Query `FAssetRegistryModule` for `FAssetData` under `folder_path`.
    2. Match asset class names against C++ prefix map `TMap<FTopLevelAssetPath, FString>`.
    3. Execute batch rename via `FAssetToolsModule::Get().RenameAssets()`.

---

### Specification 13: `find_unreferenced_assets`

* **Subsystem Name**: Asset Registry Clean-up Diagnostics
* **Source / Context**: `UnrealEngine/src/scripts/find_unreferenced_assets.py`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  ar = unreal.AssetRegistryHelpers.get_asset_registry()
  referencers = ar.get_referencers(asset_data.package_name, options)
  ```
* **Reason Native Actions Are Insufficient**: No native tool queries `IAssetRegistry` referencers to find unreferenced or orphan assets.
* **Proposed Action Specification**:
  * **Route Name**: `find_unreferenced_assets`
  * **Action Module**: `FAgentFrameworkDiagnosticsActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "folder_path": { "type": "string", "description": "Package directory path to scan" },
        "include_soft_references": { "type": "boolean", "default": true }
      },
      "required": ["folder_path"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "unreferenced_assets": { "type": "array", "items": { "type": "string" } },
        "count": { "type": "integer" },
        "ResultMessage": { "type": "string" }
      },
      "required": ["bSuccess", "unreferenced_assets", "count", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Query `IAssetRegistry::Get()` for all packages under `folder_path`.
    2. Call `GetReferencers()` for each package name.
    3. Return array of packages with zero external referencers.

---

### Specification 14: `organize_assets_by_type`

* **Subsystem Name**: Project Organization & Asset Management
* **Source / Context**: `UnrealEngine/src/scripts/organize_assets_by_type.py`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  # Class-to-folder mapping in Python
  subfolder = CLASS_TO_FOLDER.get(asset_class)
  unreal.EditorAssetLibrary.rename_asset(asset_path, new_path)
  ```
* **Reason Native Actions Are Insufficient**: No native mechanism to automatically sort mixed assets into type-specific target subfolders (`Blueprints/`, `Materials/`, `Meshes/`, `Audio/`, `Effects/`, `Maps/`, `UI/`).
* **Proposed Action Specification**:
  * **Route Name**: `organize_assets_by_type`
  * **Action Module**: `FAgentFrameworkContextActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "folder_path": { "type": "string", "description": "Target content folder path to organize" },
        "recursive": { "type": "boolean", "default": true }
      },
      "required": ["folder_path"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "moved_assets_count": { "type": "integer" },
        "category_counts": { "type": "object", "additionalProperties": { "type": "integer" } },
        "ResultMessage": { "type": "string" }
      },
      "required": ["bSuccess", "moved_assets_count", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Scan asset registry for `FAssetData` under `folder_path`.
    2. Map class names to target subfolder strings (`Blueprints/`, `Materials/`, etc.).
    3. Execute batch renames via `FAssetToolsModule::Get().RenameAssets()`.

---

### Specification 15: `inspect_uobject_properties`

* **Subsystem Name**: Core Reflection & Live Object Diagnostics
* **Source / Context**: `Tests/test_e2e_integration.py` (`test_cpp_mcp_execute_python_script_validation`)
* **Current Python Fallback Snippet**:
  ```python
  response = mock_agent_client.call_cpp_tool(
      "execute_python_script",
      { "script": "print('hello')", "justification_why_native_tools_or_skills_are_insufficient": "We need to run custom Python reflection..." }
  )
  ```
* **Reason Native Actions Are Insufficient**: `get_blueprint_schema` handles Blueprint assets and `get_cpp_reflection_info` inspects C++ headers, but no tool serializes live property values of arbitrary non-Blueprint `UObject` instances (e.g. `UDataAsset`, `USoundBase`, `UWorld` sub-objects).
* **Proposed Action Specification**:
  * **Route Name**: `inspect_uobject_properties`
  * **Action Module**: `FAgentFrameworkDiagnosticsActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "object_path": { "type": "string", "description": "Full object path or package path (e.g. /Game/Data/DA_Config.DA_Config)" },
        "include_inherited": { "type": "boolean", "default": true }
      },
      "required": ["object_path"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "object_path": { "type": "string" },
        "object_class": { "type": "string" },
        "properties": { "type": "object", "additionalProperties": true },
        "ResultMessage": { "type": "string" }
      },
      "required": ["bSuccess", "object_path", "object_class", "properties", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load target `UObject` via `StaticLoadObject`.
    2. Iterate properties via `TFieldIterator<FProperty>(TargetObject->GetClass())`.
    3. Serialize values to JSON using `FJsonObjectConverter::UStructToJsonObject` or custom property formatters.

---

### Specification 16: `set_widget_slot_properties`

* **Subsystem Name**: UMG Widget Layout & Slot Manipulation
* **Source / Context**: `blueprint-authoring` skill & `AgentFrameworkActions/Widget`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  widget_obj = unreal.load_object(None, '/Game/UI/W_MyWidget.W_MyWidget:WidgetTree.SubWidgetName')
  slot = widget_obj.slot
  slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0), maximum=unreal.Vector2D(1,1)))
  ```
* **Reason Native Actions Are Insufficient**: `FAgentFrameworkWidgetActions` provides widget blueprint creation, but lacks granular actions to modify sub-widget Slot properties (`UCanvasPanelSlot`, `UHorizontalBoxSlot`, `UVerticalBoxSlot`), anchors, alignment, or padding directly.
* **Proposed Action Specification**:
  * **Route Name**: `set_widget_slot_properties`
  * **Action Module**: `FAgentFrameworkWidgetActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "widget_blueprint_path": { "type": "string", "description": "Widget Blueprint asset path" },
        "widget_name": { "type": "string", "description": "Name of child widget inside WidgetTree" },
        "anchors": {
          "type": "object",
          "properties": { "min_x": { "type": "number" }, "min_y": { "type": "number" }, "max_x": { "type": "number" }, "max_y": { "type": "number" } }
        },
        "alignment": {
          "type": "object",
          "properties": { "x": { "type": "number" }, "y": { "type": "number" } }
        },
        "offsets": {
          "type": "object",
          "properties": { "left": { "type": "number" }, "top": { "type": "number" }, "right": { "type": "number" }, "bottom": { "type": "number" } }
        }
      },
      "required": ["widget_blueprint_path", "widget_name"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "widget_blueprint_path": { "type": "string" },
        "widget_name": { "type": "string" },
        "ResultMessage": { "type": "string" }
      },
      "required": ["bSuccess", "widget_name", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load `UWidgetBlueprint` asset.
    2. Traverse `WidgetTree` to find child `UWidget*` with matching `widget_name`.
    3. Cast `Widget->Slot` to `UCanvasPanelSlot*` (or appropriate slot class), update `FAnchorData` / alignment, mark modified and recompile.

---

### Specification 17: `invoke_pie_widget_delegate` & `get_active_runtime_widgets`

* **Subsystem Name**: Runtime Play-In-Editor (PIE) UMG Widget Query & Direct Script Delegate Invocation
* **Source / Context**: `unreal-testing-sops` skill & `AgentFrameworkActions/PIE`
* **Current Python Fallback Snippet**:
  ```python
  import unreal
  game_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  widgets = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(game_world, unreal.UserWidget, True)
  for w in widgets:
      if w.get_name() == "W_MainMenu_C":
          w.get_editor_property("PvPButton").on_clicked.broadcast()
  ```
* **Reason Native Actions Are Insufficient**: Slate hit-testing (`trigger_ui_element`) fails on occluded/collapsed widgets. No C++ action queries runtime `UUserWidget` instances during PIE or triggers script delegates directly.
* **Proposed Action Specification (Delegate Invocation)**:
  * **Route Name**: `invoke_pie_widget_delegate`
  * **Action Module**: `FAgentFrameworkPIEActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "widget_class_or_name": { "type": "string", "description": "Class or instance name of target UUserWidget" },
        "widget_property_name": { "type": "string", "description": "Child widget property name (e.g. 'PvPButton')" },
        "delegate_name": { "type": "string", "default": "OnClicked", "description": "Multicast delegate property to broadcast" }
      },
      "required": ["widget_class_or_name", "widget_property_name"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "widget_class_or_name": { "type": "string" },
        "widget_property_name": { "type": "string" },
        "delegate_name": { "type": "string" },
        "ResultMessage": { "type": "string" }
      },
      "required": ["bSuccess", "widget_class_or_name", "widget_property_name", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Obtain active PIE `UWorld` pointer via `GEditor->GetPIEWorldContext()->World()`.
    2. Call `UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, UUserWidget::StaticClass(), FoundWidgets, true)`.
    3. Filter target instance, find child `UWidget` property via reflection.
    4. Resolve `FMulticastDelegateProperty*` and invoke `ProcessMulticastDelegate(nullptr)`.

---

### Specification 18: `add_blueprint_component`

* **Subsystem Name**: Atomic Design-Time SCS Component Attachment
* **Source / Context**: `add-component` skill & `AgentFrameworkActions/Blueprint`
* **Current Python Fallback Snippet**:
  ```python
  # Multi-step C++ constructor pattern or manual editor SCS manipulation
  ```
* **Reason Native Actions Are Insufficient**: `execute_batch_blueprint_operations` operates on graph nodes, but cannot create or attach Simple Construction Script (SCS) component nodes to a `.uasset` at design time.
* **Proposed Action Specification**:
  * **Route Name**: `add_blueprint_component`
  * **Action Module**: `FAgentFrameworkBlueprintActions`
  * **Request Payload JSON Schema**:
    ```json
    {
      "$schema": "http://json-schema.org/draft-07/schema#",
      "type": "object",
      "properties": {
        "blueprint_path": { "type": "string", "description": "Object path of target Blueprint asset" },
        "component_class": { "type": "string", "description": "Class name of component (e.g. 'UStaticMeshComponent', 'USphereComponent')" },
        "component_name": { "type": "string", "description": "Name for the new component node" },
        "parent_component_name": { "type": "string", "description": "Optional: Parent SCS component node name to attach under" }
      },
      "required": ["blueprint_path", "component_class", "component_name"]
    }
    ```
  * **Expected Return Payload JSON Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "bSuccess": { "type": "boolean" },
        "blueprint_path": { "type": "string" },
        "component_name": { "type": "string" },
        "component_class": { "type": "string" },
        "ResultMessage": { "type": "string" }
      },
      "required": ["bSuccess", "component_name", "ResultMessage"]
    }
    ```
  * **C++ Implementation Approach**:
    1. Load `UBlueprint` asset.
    2. Access `USimpleConstructionScript* SCS = BP->SimpleConstructionScript;`
    3. Call `USCS_Node* NewNode = SCS->CreateNode(ComponentClass, *ComponentName);`
    4. Attach to root or specified parent node, then mark modified via `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP)`.

---

## Section 5: Implementation Roadmap & Execution Priorities

To systematically eliminate Python fallbacks and deploy these proposed native C++ actions, Phase 2 implementation should follow a prioritized 3-tier sequence based on authoring impact, token reduction, and architectural dependency:

```
┌────────────────────────────────────────────────────────────────────────┐
│ Tier 1: Core Authoring & Efficiency (High-Frequency Impact)            │
│  - disconnect_blueprint_pins                                           │
│  - create_pbr_material_from_textures                                   │
│  - modify_blueprint_subobject                                          │
│  - add_blueprint_component                                             │
│  - consolidate_asset_references                                        │
│  - inspect_uobject_properties                                          │
└────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Tier 2: Subsystem Systems Integration (Gameplay, Input & Networks)     │
│  - configure_actor_replication & set_variable_replication              │
│  - configure_input_mapping_modifiers_triggers                          │
│  - set_niagara_parameter                                               │
│  - set_widget_slot_properties                                          │
│  - enforce_naming_conventions                                          │
│  - organize_assets_by_type                                             │
└────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Tier 3: Specialized Media, Audio & PIE Testing                         │
│  - configure_sound_wave_cue                                            │
│  - MetaSound Module: create_metasound_source & wire_metasound_nodes    │
│  - invoke_pie_widget_delegate & get_active_runtime_widgets             │
│  - find_unreferenced_assets                                            │
└────────────────────────────────────────────────────────────────────────┘
```

### Verification & Validation Protocol for Phase 2 Implementation:
1. **Compilation**: After implementing each action executor in `AgentFrameworkActions`, compile the plugin via UBT using `build_plugin.ps1`.
2. **Skill Documentation Update**: Update corresponding `SKILL.md` documents in `UnrealEngine/skills/` to remove `execute_python_script` references and specify the new native MCP tools.
3. **Automated Test Execution**: Run the test wrapper `powershell -File .\Tests\run_tests.ps1` to ensure all 18 proposed action routes execute without error.
