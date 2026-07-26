# Review Report — Phase 2 Native Tools (Reviewer 2: Robustness, API Conformance, Dirty Tracking & Compilation)

## Review Summary

**Verdict**: REQUEST_CHANGES

**Summary Rationale**:
All 7 Phase 2 C++ native action tool implementations (`disconnect_blueprint_pins`, `modify_blueprint_subobject`, `configure_actor_replication`, `set_variable_replication`, `create_pbr_material_from_textures`, `create_metasound_source`, `wire_metasound_nodes`) demonstrate outstanding C++ code quality, parameter validation, alias handling (supporting both snake_case and PascalCase), error reporting (`bSuccess`, `ResultMessage`, `Errors`), dirty package tracking (`MarkBlueprintAsModified`, `MarkPackageDirty`, `AgentDirtiedPackages`), and asset compilation/rebuilding logic (`CompileAndReport`, `RecompileMaterial`, `FinishBuilding`).

However, a **Major Finding** was identified: while `material_tools.json` and `metasound_tools.json` contain complete schema specifications for `create_pbr_material_from_textures`, `create_metasound_source`, and `wire_metasound_nodes`, the 4 Phase 2 Blueprint action tools (`disconnect_blueprint_pins`, `modify_blueprint_subobject`, `configure_actor_replication`, `set_variable_replication`) are **missing from `AgentFramework/Resources/ToolSchemas/blueprint_tools.json`**. Updating `blueprint_tools.json` with schema entries for these 4 tools is required before final approval.

---

## Findings

### [Major] Finding 1: Phase 2 Blueprint Action Tool Schemas Missing from `blueprint_tools.json`

- **What**: The 4 Phase 2 Blueprint action tools (`disconnect_blueprint_pins`, `modify_blueprint_subobject`, `configure_actor_replication`, `set_variable_replication`) are implemented in C++ in `AgentFrameworkBlueprintActions.cpp`, registered in `GetSupportedToolNames()`, and dispatched in `ExecuteAction()`, but their JSON schema specifications are missing from `AgentFramework/Resources/ToolSchemas/blueprint_tools.json`.
- **Where**: `AgentFramework/Resources/ToolSchemas/blueprint_tools.json` (Lines 1–621)
- **Why**: Client AI agents and tool discovery endpoints (`list_tools_in_category`, `get_tool_info`) rely on the JSON schema declarations in `ToolSchemas/` to inspect input parameter requirements and types.
- **Suggestion**: Add the JSON schema definitions for `disconnect_blueprint_pins`, `modify_blueprint_subobject`, `configure_actor_replication`, and `set_variable_replication` to `AgentFramework/Resources/ToolSchemas/blueprint_tools.json`.

---

### [Minor] Finding 2: Inconsistent Registration in `AgentDirtiedPackages` Across Non-Blueprint Modules

- **What**: Blueprint Phase 2 actions explicitly register modified asset paths in `FAgentFrameworkActionsModule::AgentDirtiedPackages` (e.g. `AgentFrameworkBlueprintActions.cpp:5268, 5474, 5566, 5676`), whereas `create_pbr_material_from_textures` (`AgentFrameworkMaterialActions.cpp:761`) and MetaSound actions (`AgentFrameworkMetaSoundActions.cpp:247, 440`) rely solely on `Package->MarkPackageDirty()` and `Result.ModifiedAssets.Add(AssetPath)`.
- **Where**: `AgentFramework/Source/AgentFrameworkActions/Private/Material/AgentFrameworkMaterialActions.cpp` and `AgentFrameworkMetaSoundActions.cpp`
- **Why**: While `Package->MarkPackageDirty()` dirtying and package saving are functionally correct, adding `FAgentFrameworkActionsModule::AgentDirtiedPackages.Add(...)` in Material and MetaSound actions provides uniform dirty tracking across all action modules.
- **Suggestion**: Optionally append `FAgentFrameworkActionsModule::AgentDirtiedPackages.Add(FName(*AssetPath));` in `ExecuteCreatePBRMaterialFromTextures`, `ExecuteCreateMetaSoundSource`, and `ExecuteWireMetaSoundNodes`.

---

## Tool-by-Tool Detailed Review Matrix

| # | Tool Name | Module | Parameter Validation & Aliases | Status Codes & Error Messages | Dirty Package Tracking | Asset Compilation Logic | Schema Alignment |
|---|---|---|---|---|---|---|---|
| 1 | `disconnect_blueprint_pins` | Blueprint | ✅ Excellent (`asset_path`/`TargetAsset`, `node_guid`/`NodeGuid`/`node_name`/`source_node`, `pin_name`/`PinName`, `bDisconnectAll`) | ✅ Correct (`bSuccess`, `ResultMessage`, `Errors`) | ✅ `MarkBlueprintAsModified` + `AgentDirtiedPackages` | ✅ `CompileAndReport` | ❌ Missing from `blueprint_tools.json` |
| 2 | `modify_blueprint_subobject` | Blueprint | ✅ Excellent (`asset_path`/`TargetAsset`, `subobject_path`/`SubObjectPath`, `properties`/`Properties`) | ✅ Correct (`bSuccess`, `ResultMessage`, `Errors`, `Warnings`) | ✅ `MarkBlueprintAsStructurallyModified` + `AgentDirtiedPackages` | ✅ `CompileAndReport` | ❌ Missing from `blueprint_tools.json` |
| 3 | `configure_actor_replication` | Blueprint | ✅ Excellent (`asset_path`/`TargetAsset`, `bReplicates`, `bReplicateMovement`, `NetDormancy`, `NetUpdateFrequency`, `NetPriority`, checks AActor parent class) | ✅ Correct (`bSuccess`, `ResultMessage`, `Errors`) | ✅ `MarkBlueprintAsModified` + `AgentDirtiedPackages` | ✅ `CompileAndReport` | ❌ Missing from `blueprint_tools.json` |
| 4 | `set_variable_replication` | Blueprint | ✅ Excellent (`asset_path`/`TargetAsset`, `variable_name`/`VariableName`, `replication_type`/`ReplicationType`, `rep_notify_func`/`RepNotifyFunc`, `replication_condition`) | ✅ Correct (`bSuccess`, `ResultMessage`, `Errors`) | ✅ `MarkBlueprintAsStructurallyModified` + `AgentDirtiedPackages` | ✅ `CompileAndReport` + Auto RepNotify Graph | ❌ Missing from `blueprint_tools.json` |
| 5 | `create_pbr_material_from_textures` | Material | ✅ Excellent (`material_path`/`destination_path`+`material_name`, `base_color_texture_path` or `texture_maps` object, optional Normal, Roughness, Metallic, AO, Specular, Emissive, Opacity, BlendMode, ShadingModel, TwoSided) | ✅ Correct (`bSuccess`, `ResultMessage`, `Errors`, `ModifiedAssets`) | ✅ `NewMaterial->Modify()`, `Package->MarkPackageDirty()`, `SavePackage` | ✅ `RecompileMaterial` | ✅ Present in `material_tools.json` |
| 6 | `create_metasound_source` | MetaSound | ✅ Excellent (`asset_path`/`destination_path`+`asset_name`, `output_format`/`num_channels`, `is_preset`/`bIsPreset`, `preset_source_path`) | ✅ Correct (`bSuccess`, `ResultMessage`, `Errors`, `ModifiedAssets`) | ✅ `MetaSoundSource->MarkPackageDirty()` | ✅ Package Dirtying & Asset Creation | ✅ Present in `metasound_tools.json` |
| 7 | `wire_metasound_nodes` | MetaSound | ✅ Excellent (`asset_path`, `nodes_to_add`/`NodesToAdd`, `connections`/`ConnectionsToWire`/`connections_to_wire`) | ✅ Correct (`bSuccess`, `ResultMessage`, `Errors`, `Warnings`, `ModifiedAssets`) | ✅ `MetaSoundSource->Modify()`, `MetaSoundSource->MarkPackageDirty()` | ✅ `Builder.FinishBuilding()` | ✅ Present in `metasound_tools.json` |

---

## Verified Claims

- `disconnect_blueprint_pins` validates node GUIDs, pin names, breaks links, marks Blueprint modified, tracks dirty package, and compiles Blueprint -> verified via C++ inspection (`AgentFrameworkBlueprintActions.cpp:5082-5275`) -> **PASS**
- `modify_blueprint_subobject` resolves SCS component templates, WidgetTree children, CDO sub-objects, and path-based sub-objects via `StaticLoadObject`, imports properties via `ImportText_Direct`, marks modified, tracks dirty package, and compiles -> verified via C++ inspection (`AgentFrameworkBlueprintActions.cpp:5277-5481`) -> **PASS**
- `configure_actor_replication` checks `ParentClass->IsChildOf(AActor::StaticClass())`, mutates CDO replication properties, marks modified, tracks dirty package, and compiles -> verified via C++ inspection (`AgentFrameworkBlueprintActions.cpp:5483-5573`) -> **PASS**
- `set_variable_replication` mutates `FBPVariableDescription`, auto-generates RepNotify callback function graph via `CreateNewGraph`/`AddFunctionGraph`, sets ReplicationCondition metadata, marks structurally modified, tracks dirty package, and compiles -> verified via C++ inspection (`AgentFrameworkBlueprintActions.cpp:5575-5683`) -> **PASS**
- `create_pbr_material_from_textures` creates Material, instantiates `UMaterialExpressionTextureSampleParameter2D` expressions, wires PBR material properties, recompiles via `RecompileMaterial`, marks package dirty, and saves to disk -> verified via C++ inspection (`AgentFrameworkMaterialActions.cpp:520-780`) -> **PASS**
- `create_metasound_source` creates MetaSoundSource asset with selected `EMetaSoundOutputAudioFormat` or preset, and marks package dirty -> verified via C++ inspection (`AgentFrameworkMetaSoundActions.cpp:142-254`) -> **PASS**
- `wire_metasound_nodes` instantiates MetaSound graph nodes via `FMetaSoundFrontendDocumentBuilder`, wires named edges/vertices, finishes building document, and marks package dirty -> verified via C++ inspection (`AgentFrameworkMetaSoundActions.cpp:256-447`) -> **PASS**

---

## Coverage Gaps

- **Missing Blueprint Tool Schemas**: `AgentFramework/Resources/ToolSchemas/blueprint_tools.json` does not contain entries for `disconnect_blueprint_pins`, `modify_blueprint_subobject`, `configure_actor_replication`, and `set_variable_replication`. Risk level: **MEDIUM**. Recommendation: Add schema definitions to `blueprint_tools.json`.

---

## Unverified Items

- None. All 7 C++ implementation files and JSON schema definitions were fully inspected.
