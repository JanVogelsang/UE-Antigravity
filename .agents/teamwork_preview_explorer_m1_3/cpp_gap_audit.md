# C++ Action Route Gap Audit & Phase 2 Action API Specification

**Project**: UE-AgentFramework / AgentFramework  
**Target Module**: `AgentFrameworkActions` (`AgentFramework/Source/AgentFrameworkActions/`)  
**Audit Scope**: Inventory of implemented C++ actions vs. documented capabilities, skills, and Python fallbacks  
**Date**: July 26, 2026  
**Auditor**: Explorer Subagent (`teamwork_preview_explorer_m1_3`)  

---

## 1. Executive Inventory of Current C++ Action Routes

A complete audit of `AgentFrameworkActions` reveals a high-performance native architecture consisting of **27 action module directories** hosting **28 executor classes** that implement `IAgentFrameworkActionExecutor`. 

Centrally registered in `FAgentFrameworkHttpServer::RegisterAllExecutors` (`AgentFrameworkHttpServer.cpp`), these executors expose **183 discrete tool routes** over an HTTP loopback server (port `18777`).

### Complete Inventory by Action Module (183 Tools Across 27 Modules)

| # | Action Module | Executor Class | Implemented Tools Count | Tool Names Inventory |
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

## 2. Comprehensive Gap Analysis & Python Fallback Scenarios

While 181 out of 183 tools are pure native C++, analyzing agent skills (`UnrealEngine/skills/`), roadmap documents (`PLUGIN_IMPROVEMENT_ROADMAP.md`), and complex task workflows reveals **critical granular capability gaps**. These gaps force AI agents to fall back to `execute_python_script` or perform verbose multi-step workarounds.

### Key Functional Gaps Identified

1. **Blueprint Graph Pin Disconnection**:
   * *Existing Action*: `connect_blueprint_pins` connects pins.
   * *Gap*: No native tool exists to explicitly break pin links or disconnect specific pins on Blueprint nodes. Disconnecting a pin currently requires either re-injecting full T3D graph blocks or invoking Python `unreal.Blueprint` graph API calls.

2. **Protected Sub-Object Property Mutation**:
   * *Existing Action*: `set_blueprint_defaults` sets CDO root properties.
   * *Gap*: Internal sub-objects (e.g. UMG `WidgetTree` children, sub-components, or nested sub-objects) cannot be mutated via standard top-level property reflection. Skills like `blueprint-authoring` explicitly document falling back to Python colon-path loading (`unreal.load_object(None, 'Path.Path:WidgetTree.SubWidget')`).

3. **Multiplayer Replication Setup**:
   * *Existing Action*: None in `Blueprint` or `Cpp` module.
   * *Gap*: Skill `setup-replication` requires manually writing C++ code (`bReplicates = true; GetLifetimeReplicatedProps...`). Setting variable replication properties (`Replicated`, `RepNotify`, `COND_OwnerOnly`) or actor network defaults on Blueprint assets natively requires dedicated action routes.

4. **Enhanced Input Key Modifiers & Triggers**:
   * *Existing Action*: `create_input_action`, `create_input_mapping_context`, `add_input_mapping`.
   * *Gap*: `add_input_mapping` creates a basic key mapping but cannot attach key modifiers (`FEnhancedInputModifierNegate`, `FEnhancedInputModifierSwizzleAxis`) or triggers (`UInputTriggerPressed`, `UInputTriggerHold`, `UInputTriggerTap`) to individual key mappings. Configuring complex input maps forces agents to run Python scripts to construct modifier arrays.

5. **Niagara System Parameter & Curve Tuning**:
   * *Existing Action*: `set_niagara_module_pin`.
   * *Gap*: Handles module-level input pins, but cannot write system/emitter-level User parameters (`User.MyColor`, `User.SpawnRate`) or dynamic curve parameters (`UCurveFloat`, `UCurveLinearColor`) via `UNiagaraUserRedirectionParameterStore`. Agents fall back to Python script for curve and User variable binding.

6. **Generative AI PBR Material Auto-Binding**:
   * *Existing Actions*: `import_mesh`, `import_assets_batch`, `create_material`, `add_material_expression`, `connect_material_property`.
   * *Gap*: In `generate-assets`, after importing AI-generated models and PBR textures, agents must make 5-6 sequential tool calls to create nodes and connect pins individually. A single atomic C++ action `create_pbr_material_from_textures` would eliminate tool roundtrips and Python fallbacks.

7. **MetaSound Audio Graph Authoring**:
   * *Existing Actions*: None (MetaSound action module is missing, 0 tools).
   * *Gap*: Modern UE5 audio authoring relies on MetaSounds. Currently, creating or wiring MetaSound graphs requires `execute_python_script` using `IMetasoundFrontendDocumentBuilder`.

---

## 3. Proposed Native C++ Action API Specifications for Phase 2

To eliminate Python fallbacks and achieve complete native C++ capability coverage, the following **10 proposed Native C++ Action APIs** are specified for Phase 2 implementation in `AgentFrameworkActions`.

---

### API 1: `disconnect_blueprint_pins` (Module: `Blueprint`)

* **Feature / Subsystem Name**: Blueprint Graph & Node Wiring
* **Current Gap**: Unable to break specific pin connections on Blueprint nodes natively in C++ without full graph re-injection or Python scripting.
* **Proposed Route Name**: `disconnect_blueprint_pins`

#### Request Payload JSON Schema
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
      "description": "32-character hex GUID of the target node whose pin is to be disconnected"
    },
    "PinName": {
      "type": "string",
      "description": "Name of the pin to disconnect (e.g. 'Execute', 'Target', 'Output')"
    },
    "TargetNodeGuid": {
      "type": "string",
      "description": "Optional: Specific target node GUID to break connection to. If omitted, breaks all links on PinName."
    },
    "TargetPinName": {
      "type": "string",
      "description": "Optional: Specific target pin name to break connection to."
    },
    "bDisconnectAll": {
      "type": "boolean",
      "default": false,
      "description": "If true, breaks all connections on the specified pin across all target nodes."
    }
  },
  "required": ["TargetAsset", "NodeGuid", "PinName"]
}
```

#### Expected Return Payload JSON Schema
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

---

### API 2: `modify_blueprint_subobject` (Module: `Blueprint`)

* **Feature / Subsystem Name**: Blueprint Sub-Object Property Mutation
* **Current Gap**: Modifying protected internal sub-objects (e.g., UMG `WidgetTree` children or SCS sub-components) requires Python colon-path loading (`load_object(None, 'Asset.Asset:SubObject')`).
* **Proposed Route Name**: `modify_blueprint_subobject`

#### Request Payload JSON Schema
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "AssetPath": {
      "type": "string",
      "description": "Object path of the Blueprint asset (e.g. '/Game/UI/W_MainMenu')"
    },
    "SubObjectPath": {
      "type": "string",
      "description": "Relative path to sub-object (e.g. 'WidgetTree.Btn_Start' or 'SimpleConstructionScript.BoxCollider')"
    },
    "Properties": {
      "type": "object",
      "additionalProperties": true,
      "description": "Key-value map of property names to serialized values (e.g. {'ZOrder': 10, 'Visibility': 'Visible'})"
    }
  },
  "required": ["AssetPath", "SubObjectPath", "Properties"]
}
```

#### Expected Return Payload JSON Schema
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

---

### API 3: `configure_actor_replication` (Module: `Blueprint`)

* **Feature / Subsystem Name**: Actor Network Replication Defaults
* **Current Gap**: Configuring `bReplicates`, `bReplicateMovement`, `NetDormancy`, and update frequencies on Blueprint actors requires manual C++ edits or Python script execution.
* **Proposed Route Name**: `configure_actor_replication`

#### Request Payload JSON Schema
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "TargetAsset": {
      "type": "string",
      "description": "Object path of the target Blueprint actor (e.g. '/Game/Blueprints/BP_NetworkPawn')"
    },
    "bReplicates": {
      "type": "boolean",
      "default": true,
      "description": "Enable network replication for this Actor"
    },
    "bReplicateMovement": {
      "type": "boolean",
      "default": true,
      "description": "Enable automatic movement replication"
    },
    "NetDormancy": {
      "type": "string",
      "enum": ["DORM_Never", "DORM_Awake", "DORM_DormantAll", "DORM_DormantPartial", "DORM_Initial"],
      "default": "DORM_Never",
      "description": "Network dormancy state"
    },
    "NetUpdateFrequency": {
      "type": "number",
      "default": 100.0,
      "description": "Target network update frequency in Hz"
    },
    "NetPriority": {
      "type": "number",
      "default": 1.0,
      "description": "Network priority multiplier"
    }
  },
  "required": ["TargetAsset"]
}
```

#### Expected Return Payload JSON Schema
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

---

### API 4: `set_variable_replication` (Module: `Blueprint`)

* **Feature / Subsystem Name**: Blueprint Variable Replication & RepNotify
* **Current Gap**: Setting replication modes (`Replicated` vs `RepNotify`) and replication conditions on Blueprint member variables cannot be done via tool calls.
* **Proposed Route Name**: `set_variable_replication`

#### Request Payload JSON Schema
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "TargetAsset": {
      "type": "string",
      "description": "Object path of the Blueprint asset"
    },
    "VariableName": {
      "type": "string",
      "description": "Name of the target Blueprint member variable"
    },
    "ReplicationType": {
      "type": "string",
      "enum": ["None", "Replicated", "RepNotify"],
      "default": "Replicated",
      "description": "Replication mode for the variable"
    },
    "RepNotifyFunc": {
      "type": "string",
      "description": "Optional: Name of custom RepNotify callback function (auto-generated if empty and RepNotify selected)"
    },
    "ReplicationCondition": {
      "type": "string",
      "enum": ["COND_None", "COND_InitialOnly", "COND_OwnerOnly", "COND_SkipOwner", "COND_SimulatedOnly", "COND_AutonomousOnly", "COND_Custom"],
      "default": "COND_None",
      "description": "Condition governing variable replication"
    }
  },
  "required": ["TargetAsset", "VariableName", "ReplicationType"]
}
```

#### Expected Return Payload JSON Schema
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

---

### API 5: `configure_input_mapping_modifiers_triggers` (Module: `Input`)

* **Feature / Subsystem Name**: Enhanced Input Mapping Modifiers & Triggers
* **Current Gap**: `add_input_mapping` creates raw mappings but cannot configure key modifiers (`FEnhancedInputModifierNegate`, `FEnhancedInputModifierSwizzleAxis`) or triggers (`UInputTriggerPressed`, `UInputTriggerHold`), forcing Python fallbacks.
* **Proposed Route Name**: `configure_input_mapping_modifiers_triggers`

#### Request Payload JSON Schema
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "ContextAsset": {
      "type": "string",
      "description": "Object path of the UInputMappingContext asset"
    },
    "InputActionAsset": {
      "type": "string",
      "description": "Object path of the associated UInputAction asset"
    },
    "Key": {
      "type": "string",
      "description": "Key name (e.g. 'W', 'Gamepad_LeftStick_Y', 'SpaceBar')"
    },
    "Modifiers": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "Type": {
            "type": "string",
            "enum": ["Negate", "SwizzleAxis", "Scalar", "DeadZone", "Smooth", "ResponseCurve"]
          },
          "Order": { "type": "string", "enum": ["YXZ", "ZYX", "XZY"], "default": "YXZ" },
          "ScalarVector": {
            "type": "object",
            "properties": { "X": {"type": "number"}, "Y": {"type": "number"}, "Z": {"type": "number"} }
          }
        },
        "required": ["Type"]
      },
      "description": "Array of modifiers to apply to this key mapping"
    },
    "Triggers": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "Type": {
            "type": "string",
            "enum": ["Pressed", "Released", "Hold", "Tap", "Pulse", "ChordAction"]
          },
          "HoldTimeThreshold": { "type": "number", "default": 0.5 },
          "bIsOneShot": { "type": "boolean", "default": true }
        },
        "required": ["Type"]
      },
      "description": "Array of triggers governing activation"
    }
  },
  "required": ["ContextAsset", "InputActionAsset", "Key"]
}
```

#### Expected Return Payload JSON Schema
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

---

### API 6: `set_niagara_parameter` (Module: `Niagara`)

* **Feature / Subsystem Name**: Niagara System Parameters & Curves
* **Current Gap**: `set_niagara_module_pin` only affects module inputs. Setting System/Emitter-level User parameters (`User.MyColor`, `User.SpawnRate`) or dynamic time-value curves requires native `UNiagaraUserRedirectionParameterStore` routing.
* **Proposed Route Name**: `set_niagara_parameter`

#### Request Payload JSON Schema
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "SystemAsset": {
      "type": "string",
      "description": "Object path of the Niagara System asset"
    },
    "ParameterScope": {
      "type": "string",
      "enum": ["User", "System", "Emitter"],
      "default": "User",
      "description": "Scope of the Niagara parameter"
    },
    "ParameterName": {
      "type": "string",
      "description": "Name of parameter (e.g. 'SpawnRate', 'PrimaryColor', 'ScaleCurve')"
    },
    "DataType": {
      "type": "string",
      "enum": ["Float", "Vector2", "Vector3", "LinearColor", "Bool", "Int32", "CurveFloat", "CurveLinearColor"],
      "default": "Float"
    },
    "Value": {
      "description": "Constant scalar, vector object, or color object value"
    },
    "CurveKeys": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "Time": { "type": "number" },
          "Value": { "type": "number" }
        },
        "required": ["Time", "Value"]
      },
      "description": "Optional: Time-value curve keys for CurveFloat / CurveLinearColor data types"
    }
  },
  "required": ["SystemAsset", "ParameterName", "DataType"]
}
```

#### Expected Return Payload JSON Schema
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

---

### API 7: `create_pbr_material_from_textures` (Module: `Material`)

* **Feature / Subsystem Name**: Generative AI PBR Material Generation
* **Current Gap**: Building materials from imported AI PBR textures (`generate-assets` skill) requires 5-6 manual tool calls (`create_material`, multiple `add_material_expression`, pin connections). A single atomic C++ route builds the complete material graph instantly.
* **Proposed Route Name**: `create_pbr_material_from_textures`

#### Request Payload JSON Schema
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "MaterialPath": {
      "type": "string",
      "description": "Destination path for the new Material asset (e.g. '/Game/GenAI/Materials/M_RustyBarrel')"
    },
    "BaseColorTexturePath": {
      "type": "string",
      "description": "Object path of BaseColor texture asset"
    },
    "NormalTexturePath": {
      "type": "string",
      "description": "Optional: Object path of Normal map texture asset"
    },
    "RoughnessTexturePath": {
      "type": "string",
      "description": "Optional: Object path of Roughness texture asset"
    },
    "MetallicTexturePath": {
      "type": "string",
      "description": "Optional: Object path of Metallic texture asset"
    },
    "AOTexturePath": {
      "type": "string",
      "description": "Optional: Object path of Ambient Occlusion texture asset"
    },
    "BlendMode": {
      "type": "string",
      "enum": ["Opaque", "Masked", "Translucent", "Additive"],
      "default": "Opaque"
    },
    "ShadingModel": {
      "type": "string",
      "enum": ["DefaultLit", "Unlit", "Subsurface", "ClearCoat"],
      "default": "DefaultLit"
    }
  },
  "required": ["MaterialPath", "BaseColorTexturePath"]
}
```

#### Expected Return Payload JSON Schema
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

---

### API 8: `configure_sound_wave_cue` (Module: `Media`)

* **Feature / Subsystem Name**: Sound Wave & Sound Cue Asset Configuration
* **Current Gap**: Importing audio files creates `USoundWave`, but creating `USoundCue` assets with attenuation settings, pitch modulation, and looping requires Python scripts.
* **Proposed Route Name**: `configure_sound_wave_cue`

#### Request Payload JSON Schema
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "SoundWaveAsset": {
      "type": "string",
      "description": "Object path of the imported SoundWave asset (e.g. '/Game/Audio/SW_Laugh')"
    },
    "CueAssetPath": {
      "type": "string",
      "description": "Optional: Destination path to create a USoundCue wrapper (e.g. '/Game/Audio/SC_Laugh')"
    },
    "bLooping": {
      "type": "boolean",
      "default": false,
      "description": "Set sound wave looping flag"
    },
    "VolumeMultiplier": {
      "type": "number",
      "default": 1.0,
      "description": "Base volume multiplier"
    },
    "PitchMultiplier": {
      "type": "number",
      "default": 1.0,
      "description": "Base pitch multiplier"
    },
    "AttenuationAssetPath": {
      "type": "string",
      "description": "Optional: Path to USoundAttenuation asset to bind"
    }
  },
  "required": ["SoundWaveAsset"]
}
```

#### Expected Return Payload JSON Schema
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

---

### API 9: `create_metasound_source` (Module: `MetaSound` - New Executor)

* **Feature / Subsystem Name**: MetaSound Audio System Initialization
* **Current Gap**: `AgentFrameworkActions` completely lacks a MetaSound action executor module (0 tools). Creating MetaSound assets requires Python script invocation via `IMetasoundFrontendDocumentBuilder`.
* **Proposed Route Name**: `create_metasound_source`

#### Request Payload JSON Schema
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "AssetPath": {
      "type": "string",
      "description": "Destination path for the MetaSoundSource asset (e.g. '/Game/Audio/MS_ProceduralFootstep')"
    },
    "OutputFormat": {
      "type": "string",
      "enum": ["Mono", "Stereo", "Quad", "5.1", "7.1"],
      "default": "Stereo",
      "description": "Audio output channel configuration"
    },
    "bIsPreset": {
      "type": "boolean",
      "default": false,
      "description": "If true, creates MetaSound preset based on parent source"
    }
  },
  "required": ["AssetPath"]
}
```

#### Expected Return Payload JSON Schema
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

---

### API 10: `wire_metasound_nodes` (Module: `MetaSound` - New Executor)

* **Feature / Subsystem Name**: MetaSound Audio Graph Wiring
* **Current Gap**: Injecting nodes (Oscillators, Envelopes, WavePlayers) and wiring input/output audio/trigger pins inside MetaSound graphs natively in C++.
* **Proposed Route Name**: `wire_metasound_nodes`

#### Request Payload JSON Schema
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "AssetPath": {
      "type": "string",
      "description": "Object path of the target MetaSoundSource asset"
    },
    "NodesToAdd": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "NodeClassName": { "type": "string", "description": "e.g. 'WavePlayer:Mono', 'Sine:Audio', 'ADSR:Envelope'" },
          "NodeName": { "type": "string", "description": "Custom identifier for the node" }
        },
        "required": ["NodeClassName", "NodeName"]
      },
      "description": "Array of MetaSound builder nodes to add"
    },
    "ConnectionsToWire": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "FromNode": { "type": "string" },
          "FromPin": { "type": "string" },
          "ToNode": { "type": "string" },
          "ToPin": { "type": "string" }
        },
        "required": ["FromNode", "FromPin", "ToNode", "ToPin"]
      },
      "description": "Array of pin connections to wire"
    }
  },
  "required": ["AssetPath"]
}
```

#### Expected Return Payload JSON Schema
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

---

## 4. Implementation Roadmap & Phase 2 Action Plan

To systematically eliminate Python fallbacks and deploy these 10 proposed native C++ actions, Phase 2 implementation should follow a prioritized 3-tier sequence:

```
┌────────────────────────────────────────────────────────────────────────┐
│ Priority 1: Core Authoring & Efficiency (High Frequency Impact)       │
│  - disconnect_blueprint_pins                                           │
│  - create_pbr_material_from_textures                                   │
│  - modify_blueprint_subobject                                          │
└────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Priority 2: System Systems Integration (Gameplay & Network)            │
│  - configure_actor_replication & set_variable_replication              │
│  - configure_input_mapping_modifiers_triggers                          │
│  - set_niagara_parameter                                               │
└────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Priority 3: Media & Audio Subsystems (New Native Modules)              │
│  - configure_sound_wave_cue                                            │
│  - MetaSound Module: create_metasound_source & wire_metasound_nodes    │
└────────────────────────────────────────────────────────────────────────┘
```

### Verification & Validation Protocol
After implementing each native action route in Phase 2:
1. Re-compile `AgentFrameworkActions` via UBT (`build_plugin.ps1`).
2. Update corresponding `SKILL.md` documents in `UnrealEngine/skills/` to remove `execute_python_script` references.
3. Run automated E2E tests: `powershell -File .\Tests\run_tests.ps1`.
