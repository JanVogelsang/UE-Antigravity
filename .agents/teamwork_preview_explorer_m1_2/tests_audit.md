# UE-AgentFramework Tests & Python Server/Bridge Scripts Audit Report

## Executive Summary

This report presents a comprehensive audit of the `Tests/` directory, `UnrealEngine/` Python server and bridge scripts, utility scripts in `UnrealEngine/src/scripts/`, and root level automation scripts within the `UE-Antigravity` project. 

The investigation focused on identifying every instance where test workflows, developer automation scripts, or bridge/server components bypass native C++ MCP actions by invoking `execute_python_script` or Python `unreal.*` module calls. For each identified instance, this audit analyzes the root cause (missing or non-granular C++ action routes) and proposes a concrete **Native C++ Action API Specification** to replace Python execution in Phase 2.

---

## Audit Findings Overview

Across the audited files, we cataloged **7 distinct feature gaps** where Python script execution (`execute_python_script` or `unreal.*` bindings) is used due to absent or non-granular native C++ MCP tool endpoints:

| # | Feature / Subsystem Name | File Path & Context | Primary Python API Used | Root Cause for Python Usage | Proposed Native C++ Action Tool |
|---|---|---|---|---|---|
| 1 | Asset Reference Replacement & Consolidation | `UnrealEngine/src/scripts/bulk_replace_references.py` | `unreal.EditorAssetLibrary.consolidate_assets` | Missing native C++ asset reference consolidation route | `consolidate_asset_references` |
| 2 | Automated Asset Naming Standard Enforcement | `UnrealEngine/src/scripts/clean_naming_conventions.py` | `unreal.EditorAssetLibrary.list_assets`, `rename_asset` | Lack of batch asset renaming with convention matching in C++ | `enforce_naming_conventions` |
| 3 | Unreferenced Asset Auditing | `UnrealEngine/src/scripts/find_unreferenced_assets.py` | `unreal.AssetRegistryHelpers.get_asset_registry` | Missing dependency graph / referencer query endpoints in C++ | `find_unreferenced_assets` |
| 4 | Asset Restructuring by Class Type | `UnrealEngine/src/scripts/organize_assets_by_type.py` | `unreal.EditorAssetLibrary.list_assets`, `find_asset_data` | Lack of automated subfolder restructuring C++ action | `organize_assets_by_type` |
| 5 | Non-Blueprint UObject Reflection & Metadata Inspection | `Tests/test_e2e_integration.py` (`test_cpp_mcp_execute_python_script_validation`) | `execute_python_script` | No C++ tool to read live property metadata of arbitrary UObjects | `inspect_uobject_properties` |
| 6 | UMG Slot Property & Anchor Wiring | `UnrealEngine/skills/blueprint-authoring/SKILL.md` (and UI scripts) | `unreal.load_object`, `slot.set_anchors` | `FAgentFrameworkWidgetActions` lacks slot alignment/anchor manipulation | `set_widget_slot_properties` |
| 7 | Active Runtime Widget Inspection in PIE | `UnrealEngine/skills/unreal-testing-sops/SKILL.md` (and PIE SOPs) | `unreal.WidgetBlueprintLibrary.get_all_widgets_of_class` | No native C++ action to query runtime UUserWidget instances during PIE | `get_active_runtime_widgets` |

---

## Detailed Instance Analysis & Proposed C++ API Specifications

### Instance 1: Asset Reference Replacement & Consolidation

- **Feature / Subsystem Name**: Asset Management & Consolidation
- **File Path & Module**: `UnrealEngine/src/scripts/bulk_replace_references.py` (`bulk_replace_references`)
- **Current Python Fallback Implementation Snippet**:
  ```python
  import unreal

  def bulk_replace_references(source_path, target_path):
      if not unreal.EditorAssetLibrary.does_asset_exist(source_path):
          return False
      if not unreal.EditorAssetLibrary.does_asset_exist(target_path):
          return False
      source_asset = unreal.EditorAssetLibrary.load_asset(source_path)
      target_asset = unreal.EditorAssetLibrary.load_asset(target_path)
      success = unreal.EditorAssetLibrary.consolidate_assets(target_asset, [source_asset])
      return success
  ```
- **Reason Native C++ Actions Are Insufficient**:
  `FAgentFrameworkContextActions` and `FAgentFrameworkLevelActions` do not expose an endpoint to perform reference consolidation (`ObjectTools::ConsolidateObjects` or `UEditorAssetLibrary::ConsolidateAssets`). When agents or developers need to merge duplicate materials, textures, or Blueprints, they are forced to execute Python scripts to invoke `unreal.EditorAssetLibrary.consolidate_assets`.
- **Proposed Native C++ Action API Specification**:
  - **Action Executor Module**: `FAgentFrameworkContextActions` (or `FAgentFrameworkAssetActions`)
  - **Tool Name**: `consolidate_asset_references`
  - **Input JSON Schema**:
    ```json
    {
      "name": "consolidate_asset_references",
      "description": "Consolidate assets by replacing all references to the source asset with references to the target asset, then deleting the source asset.",
      "parameters": {
        "type": "object",
        "properties": {
          "source_asset_path": { "type": "string", "description": "Package path of the asset to be replaced and deleted (e.g. /Game/OldMat)" },
          "target_asset_path": { "type": "string", "description": "Package path of the asset to replace references with (e.g. /Game/NewMat)" }
        },
        "required": ["source_asset_path", "target_asset_path"]
      }
    }
    ```
  - **C++ Implementation Plan**:
    1. Load source and target `UObject` pointers using `StaticLoadObject`.
    2. Check object compatibility and validity.
    3. Invoke `UEditorAssetLibrary::ConsolidateAssets(TargetAsset, { SourceAsset })` or `ObjectTools::ConsolidateObjects`.
    4. Return structured JSON result: `{"bSuccess": true, "ResultMessage": "Successfully consolidated references from /Game/OldMat to /Game/NewMat."}`.

---

### Instance 2: Automated Asset Naming Standard Enforcement

- **Feature / Subsystem Name**: Asset Hygiene & Naming Conventions
- **File Path & Module**: `UnrealEngine/src/scripts/clean_naming_conventions.py` (`clean_naming_conventions`)
- **Current Python Fallback Implementation Snippet**:
  ```python
  import unreal

  PREFIXES = {
      "Blueprint": "BP_", "Texture2D": "T_", "Material": "M_",
      "MaterialInstanceConstant": "MI_", "StaticMesh": "SM_",
      "SkeletalMesh": "SK_", "NiagaraSystem": "NS_", "WidgetBlueprint": "WBP_"
  }

  def clean_naming_conventions(folder_path):
      assets = unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True)
      for asset_path in assets:
          asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
          prefix = PREFIXES.get(asset_class)
          if prefix and not asset_name.startswith(prefix):
              new_asset_path = f"{parent_path}/{prefix}{asset_name}"
              unreal.EditorAssetLibrary.rename_asset(asset_path, new_asset_path)
  ```
- **Reason Native C++ Actions Are Insufficient**:
  `FAgentFrameworkContextActions` provides single asset rename operations, but lacks batch evaluation of UE5 asset naming standards across directory trees with automated prefix matching and suffix conflict resolution.
- **Proposed Native C++ Action API Specification**:
  - **Action Executor Module**: `FAgentFrameworkContextActions`
  - **Tool Name**: `enforce_naming_conventions`
  - **Input JSON Schema**:
    ```json
    {
      "name": "enforce_naming_conventions",
      "description": "Scans a content directory recursively and renames assets to conform to standard UE5 class prefixes (BP_, SM_, T_, M_, MI_, NS_, WBP_, etc.).",
      "parameters": {
        "type": "object",
        "properties": {
          "folder_path": { "type": "string", "description": "Package directory path to scan (e.g. /Game/UI)" },
          "recursive": { "type": "boolean", "default": true, "description": "Whether to scan subdirectories recursively" },
          "dry_run": { "type": "boolean", "default": false, "description": "If true, returns proposed renames without executing them" }
        },
        "required": ["folder_path"]
      }
    }
    ```
  - **C++ Implementation Plan**:
    1. Use `FAssetRegistryModule` to gather `FAssetData` array under `folder_path`.
    2. Maintain internal C++ `TMap<FTopLevelAssetPath, FString>` mapping class names to standard prefixes.
    3. Calculate required name changes, resolving collisions via `FAssetToolsModule::Get().RenameAssets()`.
    4. Return summary JSON containing list of renamed assets and failure counts.

---

### Instance 3: Unreferenced Asset Auditing & Garbage Identification

- **Feature / Subsystem Name**: Asset Registry & Clean-up Diagnostics
- **File Path & Module**: `UnrealEngine/src/scripts/find_unreferenced_assets.py` (`find_unreferenced_assets`)
- **Current Python Fallback Implementation Snippet**:
  ```python
  import unreal

  def find_unreferenced_assets(folder_path):
      ar = unreal.AssetRegistryHelpers.get_asset_registry()
      assets = unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True)
      unreferenced = []
      for asset_path in assets:
          asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
          referencers = ar.get_referencers(asset_data.package_name, options)
          if len([r for r in referencers if str(r) != str(asset_data.package_name)]) == 0:
              unreferenced.append(asset_path)
      return unreferenced
  ```
- **Reason Native C++ Actions Are Insufficient**:
  `FAgentFrameworkDiagnosticsActions` and `FAgentFrameworkContextActions` do not provide referencer graph lookup functions. Agents attempting project cleanup or asset auditing must run Python scripts calling `IAssetRegistry::GetReferencers()`.
- **Proposed Native C++ Action API Specification**:
  - **Action Executor Module**: `FAgentFrameworkDiagnosticsActions` (or `FAgentFrameworkAssetActions`)
  - **Tool Name**: `find_unreferenced_assets`
  - **Input JSON Schema**:
    ```json
    {
      "name": "find_unreferenced_assets",
      "description": "Scans a content directory recursively and returns a list of assets that have zero external referencers (dependencies pointing to them).",
      "parameters": {
        "type": "object",
        "properties": {
          "folder_path": { "type": "string", "description": "Package directory path to scan (e.g. /Game/UnusedAssets)" },
          "include_soft_references": { "type": "boolean", "default": true, "description": "Whether to consider soft package references as valid referencers" }
        },
        "required": ["folder_path"]
      }
    }
    ```
  - **C++ Implementation Plan**:
    1. Query `IAssetRegistry::Get()` for all asset package names in `folder_path`.
    2. Construct `UE::AssetRegistry::EDependencyQuery` options based on parameters.
    3. Call `GetReferencers()` for each package and check for external dependencies.
    4. Return JSON response array `{"unreferenced_assets": ["/Game/UnusedAssets/T_Unused.T_Unused"], "count": 1}`.

---

### Instance 4: Automatic Asset Directory Restructuring by Type

- **Feature / Subsystem Name**: Project Organization & Asset Management
- **File Path & Module**: `UnrealEngine/src/scripts/organize_assets_by_type.py` (`organize_assets_by_type`)
- **Current Python Fallback Implementation Snippet**:
  ```python
  import unreal

  CLASS_TO_FOLDER = {
      "Blueprint": "Blueprints", "Texture2D": "Textures", "Material": "Materials",
      "StaticMesh": "Meshes", "NiagaraSystem": "Effects", "SoundWave": "Audio", "World": "Maps"
  }

  def organize_assets_by_type(folder_path):
      assets = unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True)
      for asset_path in assets:
          asset_data = unreal.EditorAssetLibrary.find_asset_data(asset_path)
          subfolder = CLASS_TO_FOLDER.get(asset_class)
          if subfolder:
              new_path = f"{folder_path}/{subfolder}/{asset_name}"
              unreal.EditorAssetLibrary.rename_asset(asset_path, new_path)
  ```
- **Reason Native C++ Actions Are Insufficient**:
  While `FAgentFrameworkContextActions` offers basic directory search, it has no native C++ mechanism to sort mixed assets into standard target subfolders (`Blueprints/`, `Materials/`, `Meshes/`, `Audio/`, `UI/`, `Effects/`, `Maps/`) by class.
- **Proposed Native C++ Action API Specification**:
  - **Action Executor Module**: `FAgentFrameworkContextActions`
  - **Tool Name**: `organize_assets_by_type`
  - **Input JSON Schema**:
    ```json
    {
      "name": "organize_assets_by_type",
      "description": "Organizes mixed assets in a directory into type-specific subfolders (Blueprints, Textures, Materials, Meshes, Audio, Effects, Maps, UI).",
      "parameters": {
        "type": "object",
        "properties": {
          "folder_path": { "type": "string", "description": "Target content folder path to organize" },
          "recursive": { "type": "boolean", "default": true, "description": "Process subdirectories recursively" }
        },
        "required": ["folder_path"]
      }
    }
    ```
  - **C++ Implementation Plan**:
    1. Scan asset registry for `FAssetData` under `folder_path`.
    2. Map asset class names to subfolder names using internal `TMap<FString, FString>`.
    3. Construct target package paths and invoke `FAssetToolsModule::Get().RenameAssets()`.
    4. Return JSON response detailing moved asset counts per category.

---

### Instance 5: Non-Blueprint UObject Reflection & Metadata Inspection

- **Feature / Subsystem Name**: Core Reflection & Diagnostics (Test Suite Validation Workflow)
- **File Path & Module**: `Tests/test_e2e_integration.py` (`test_cpp_mcp_execute_python_script_validation`)
- **Current Python Fallback Implementation Snippet**:
  ```python
  response = mock_agent_client.call_cpp_tool(
      "execute_python_script",
      {
          "script": "print('hello')",
          "justification_why_native_tools_or_skills_are_insufficient": "We need to run custom Python reflection because no native tool can read metadata of non-blueprint UObjects"
      }
  )
  ```
- **Reason Native C++ Actions Are Insufficient**:
  `FAgentFrameworkBlueprintActions::get_blueprint_schema` handles Blueprint assets, and `get_cpp_reflection_info` inspects class declarations, but no native C++ tool allows agents to inspect live property values and metadata of instantiated non-Blueprint `UObject` instances (e.g. `UDataAsset`, `USoundBase`, `UWorld` sub-objects, or custom `UObject` subclasses).
- **Proposed Native C++ Action API Specification**:
  - **Action Executor Module**: `FAgentFrameworkDiagnosticsActions`
  - **Tool Name**: `inspect_uobject_properties`
  - **Input JSON Schema**:
    ```json
    {
      "name": "inspect_uobject_properties",
      "description": "Reads and serializes the live property values and metadata of any UObject instance or asset object.",
      "parameters": {
        "type": "object",
        "properties": {
          "object_path": { "type": "string", "description": "Full object path or package path (e.g. /Game/Data/DA_Config.DA_Config)" },
          "include_inherited": { "type": "boolean", "default": true, "description": "Whether to include inherited UPROPERTY fields" }
        },
        "required": ["object_path"]
      }
    }
    ```
  - **C++ Implementation Plan**:
    1. Load target `UObject` using `StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath)`.
    2. Iterate over properties via `TFieldIterator<FProperty>(TargetObject->GetClass())`.
    3. Serialize property values into JSON key-value pairs using `FJsonObjectConverter::UStructToJsonObject` or custom property formatters.
    4. Return JSON response containing object class, path, and key-value property map.

---

### Instance 6: Direct Sub-object Property Wiring in UMG UI Editor

- **Feature / Subsystem Name**: Widget & UMG Layout Manipulation
- **File Path & Module**: `UnrealEngine/skills/blueprint-authoring/SKILL.md` (and widget authoring procedures)
- **Current Python Fallback Implementation Snippet**:
  ```python
  import unreal
  widget_obj = unreal.load_object(None, '/Game/UI/W_MyWidget.W_MyWidget:WidgetTree.SubWidgetName')
  slot = widget_obj.slot
  slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0), maximum=unreal.Vector2D(1,1)))
  ```
- **Reason Native C++ Actions Are Insufficient**:
  `FAgentFrameworkWidgetActions` provides widget blueprint creation (`create_widget_blueprint`, `add_widget_to_canvas`), but lacks granular actions to modify sub-widget Slot properties (`UCanvasPanelSlot`, `UHorizontalBoxSlot`, `UVerticalBoxSlot`), set anchors, alignment, or padding directly on nested widget trees without writing Python code using `unreal.load_object`.
- **Proposed Native C++ Action API Specification**:
  - **Action Executor Module**: `FAgentFrameworkWidgetActions`
  - **Tool Name**: `set_widget_slot_properties`
  - **Input JSON Schema**:
    ```json
    {
      "name": "set_widget_slot_properties",
      "description": "Sets layout slot properties (anchors, alignment, offsets, size) for a child widget inside a Widget Blueprint tree.",
      "parameters": {
        "type": "object",
        "properties": {
          "widget_blueprint_path": { "type": "string", "description": "Widget Blueprint asset path (e.g. /Game/UI/W_MainHUD)" },
          "widget_name": { "type": "string", "description": "Name of child widget inside WidgetTree" },
          "anchors": {
            "type": "object",
            "properties": {
              "min_x": { "type": "number" }, "min_y": { "type": "number" },
              "max_x": { "type": "number" }, "max_y": { "type": "number" }
            }
          },
          "alignment": {
            "type": "object",
            "properties": {
              "x": { "type": "number" }, "y": { "type": "number" }
            }
          },
          "offsets": {
            "type": "object",
            "properties": {
              "left": { "type": "number" }, "top": { "type": "number" },
              "right": { "type": "number" }, "bottom": { "type": "number" }
            }
          }
        },
        "required": ["widget_blueprint_path", "widget_name"]
      }
    }
    ```
  - **C++ Implementation Plan**:
    1. Load `UWidgetBlueprint` asset.
    2. Traverse `WidgetTree` to find `UWidget*` with matching `widget_name`.
    3. Access `Widget->Slot`, cast to `UCanvasPanelSlot*` (or appropriate slot class).
    4. Apply `FAnchorData` / alignment updates, call `FBlueprintEditorUtils::MarkBlueprintAsModified()`, and recompile blueprint.

---

### Instance 7: Active Runtime Widget Inspection in PIE

- **Feature / Subsystem Name**: Automated UI Testing & PIE Diagnostics
- **File Path & Module**: `UnrealEngine/skills/unreal-testing-sops/SKILL.md` (and testing SOPs)
- **Current Python Fallback Implementation Snippet**:
  ```python
  import unreal
  editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
  game_world = editor_subsystem.get_game_world()
  widgets = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(game_world, unreal.UserWidget, True)
  ```
- **Reason Native C++ Actions Are Insufficient**:
  `FAgentFrameworkWidgetActions::extract_ui_state` retrieves static Slate/UMG widget layouts, but automated testing SOPs requiring dynamic retrieval of active `UUserWidget` runtime instances filtered by class or visibility during Play-In-Editor (PIE) rely on Python calls to `UWidgetBlueprintLibrary::GetAllWidgetsOfClass`.
- **Proposed Native C++ Action API Specification**:
  - **Action Executor Module**: `FAgentFrameworkWidgetActions` (or `FAgentFrameworkDiagnosticsActions`)
  - **Tool Name**: `get_active_runtime_widgets`
  - **Input JSON Schema**:
    ```json
    {
      "name": "get_active_runtime_widgets",
      "description": "Retrieves all active UUserWidget runtime instances in the PIE world, filtered by widget class and visibility.",
      "parameters": {
        "type": "object",
        "properties": {
          "widget_class_filter": { "type": "string", "default": "UserWidget", "description": "Class name filter for widgets" },
          "top_level_only": { "type": "boolean", "default": false, "description": "If true, only returns top-level viewport widgets" }
        }
      }
    }
    ```
  - **C++ Implementation Plan**:
    1. Get PIE `UWorld` pointer via `GEditor->GetPIEWorldContext()`.
    2. Invoke `UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, TargetClass, TopLevelOnly, OutWidgets)`.
    3. Serialize widget class names, instance names, visibility state, and viewport bounds into JSON array.
    4. Return JSON response `{ "widgets": [ { "name": "W_Inventory_C_0", "class": "W_Inventory_C", "visible": true } ] }`.

---

## Strategic Summary & Phase 2 Migration Roadmap

By implementing these 7 proposed C++ tools in Phase 2, `UE-AgentFramework` will achieve complete replacement of `execute_python_script` and `unreal.*` Python module dependencies across the `Tests/` directory, developer utilities in `UnrealEngine/src/scripts/`, and skill automation procedures.

### Summary Matrix of Proposed Actions

| Tool Name | Domain / Executor Class | Target Capabilities Replaced | Complexity | Priority |
|---|---|---|---|---|
| `consolidate_asset_references` | `FAgentFrameworkContextActions` | Merging duplicate assets & replacing references | Low | High |
| `enforce_naming_conventions` | `FAgentFrameworkContextActions` | Batch UE5 naming standard enforcement (`BP_`, `SM_`, etc.) | Medium | High |
| `find_unreferenced_assets` | `FAgentFrameworkDiagnosticsActions` | Asset registry referencer queries & unreferenced scans | Medium | Medium |
| `organize_assets_by_type` | `FAgentFrameworkContextActions` | Automated folder sorting by asset class type | Low | Medium |
| `inspect_uobject_properties` | `FAgentFrameworkDiagnosticsActions` | Live UObject instance property serialization | Medium | High |
| `set_widget_slot_properties` | `FAgentFrameworkWidgetActions` | UMG Sub-widget Slot anchors, padding, & alignment | High | High |
| `get_active_runtime_widgets` | `FAgentFrameworkWidgetActions` | PIE runtime UUserWidget instance enumeration | Medium | Medium |

---
*Report generated by Explorer Subagent for Milestone M1.2 Audit.*
