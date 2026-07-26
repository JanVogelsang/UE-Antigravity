# Phase 3: Skill & Test Suite Migration Analysis Report

## Document Information
- **Project**: UE-AgentFramework (`c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity`)
- **Target Output**: `.agents/explorer_phase3/analysis.md`
- **Milestone**: Phase 3 Skill & Test Suite Migration Analysis
- **Date**: July 26, 2026
- **Status**: Complete Analysis & Implementation Blueprint

---

## Executive Summary

This report provides a comprehensive analysis and actionable implementation plan for **Phase 3 (Skill & Test Suite Migration)** of `UE-AgentFramework`. Building upon the 18 native C++ action specifications developed during Phase 1 (`PYTHON_FALLBACK_AUDIT.md`) and implemented in Phase 2 (`PLUGIN_IMPROVEMENT_ROADMAP.md`), Phase 3 migrates all remaining agent skills, developer utility scripts, and test suites away from Python fallbacks (`execute_python_script` / `unreal.*` modules) to native C++ MCP Editor tool calls operating on port `18777`.

---

## Section 1: R1 — Skill Updates Audit & Specifications (`UnrealEngine/skills/`)

### 1.1 Target Skills Overview & Audit Matrix

All 7 target skills in `UnrealEngine/skills/` were inspected line-by-line to evaluate their current native tool alignment and identify required documentation enhancements:

| # | Skill Folder | File Path | Current Status | Native Tool Replacements / Enhancements Required |
|---|---|---|---|---|
| 1 | `blueprint-authoring` | `UnrealEngine/skills/blueprint-authoring/SKILL.md` | Partially Migrated | Add `disconnect_blueprint_pins` native tool specification to Step 4 (Pin Disconnection). |
| 2 | `unreal-testing-sops` | `UnrealEngine/skills/unreal-testing-sops/SKILL.md` | Partially Migrated | Document `invoke_pie_widget_delegate` and `get_active_runtime_widgets` in Option C for PIE UI testing. |
| 3 | `add-component` | `UnrealEngine/skills/add-component/SKILL.md` | Fully Migrated | Documented `add_blueprint_component` for design-time SCS component attachment. |
| 4 | `generate-assets` | `UnrealEngine/skills/generate-assets/SKILL.md` | Fully Migrated | Uses `create_pbr_material_from_textures` and `configure_sound_wave_cue` for in-editor asset setup. |
| 5 | `setup-input` | `UnrealEngine/skills/setup-input/SKILL.md` | Fully Migrated | Uses `create_input_action`, `add_input_mapping`, and `configure_input_mapping_modifiers_triggers`. |
| 6 | `setup-replication` | `UnrealEngine/skills/setup-replication/SKILL.md` | Fully Migrated | Uses `configure_actor_replication` and `set_variable_replication`. |
| 7 | `niagara-authoring` | `UnrealEngine/skills/niagara-authoring/SKILL.md` | Fully Migrated | Uses `set_niagara_parameter` for User parameters and float curves. |

---

### 1.2 Detailed Skill Edits & Code Snippet Specifications

#### 1. Skill: `blueprint-authoring` (`UnrealEngine/skills/blueprint-authoring/SKILL.md`)
* **Observation**: Step 4 currently specifies `connect_blueprint_pins` for connecting pins, but lacks documentation on how to break links or disconnect specific pins using `disconnect_blueprint_pins`.
* **Required Edits**: Update Step 4 and add a pin disconnection section.
* **Exact Content to Inject into `blueprint-authoring/SKILL.md`**:
  ```markdown
  ### Pin Connection & Disconnection Tools
  * **Connect Pins (`connect_blueprint_pins`)**: Connect output pin on source node to input pin on target node.
  * **Disconnect Pins (`disconnect_blueprint_pins`)**: Disconnect specific pin links or break all connections on a pin:
    ```json
    {
      "TargetAsset": "/Game/Blueprints/BP_Player",
      "NodeGuid": "3E2A5D8446B84A29B52C2D812A2BD5F5",
      "PinName": "Execute",
      "bDisconnectAll": true
    }
    ```
  ```

#### 2. Skill: `unreal-testing-sops` (`UnrealEngine/skills/unreal-testing-sops/SKILL.md`)
* **Observation**: SOP-001 Option C includes `invoke_pie_widget_delegate` and `get_active_runtime_widgets`. SOP-002 Step 5 still references a local python script `parse_benchmark.py` for benchmark output formatting.
* **Required Edits**: Validate Option C and ensure runtime widget delegate invocation (`invoke_pie_widget_delegate`) is highlighted as the primary non-interactive UI testing mechanism.

#### 3. Skill: `add-component` (`UnrealEngine/skills/add-component/SKILL.md`)
* **Observation**: Fully updated with `add_blueprint_component` documentation.
* **Specification Verification**:
  ```json
  {
    "blueprint_path": "/Game/Blueprints/BP_MyActor",
    "component_class": "UStaticMeshComponent",
    "component_name": "MeshComponent",
    "parent_component_name": "DefaultSceneRoot"
  }
  ```

#### 4. Skill: `generate-assets` (`UnrealEngine/skills/generate-assets/SKILL.md`)
* **Observation**: External asset generation (Meshy/ElevenLabs) uses `generative_utils.py` (CLI), while all Unreal Editor operations are 100% native:
  * `import_mesh` & `import_assets_batch` for asset import.
  * `create_pbr_material_from_textures` for atomic PBR material building.
  * `configure_sound_wave_cue` for audio playback and Sound Cue creation.

#### 5. Skill: `setup-input` (`UnrealEngine/skills/setup-input/SKILL.md`)
* **Observation**: Fully documents Enhanced Input setup using `create_input_action`, `create_input_mapping_context`, `add_input_mapping`, and `configure_input_mapping_modifiers_triggers`.

#### 6. Skill: `setup-replication` (`UnrealEngine/skills/setup-replication/SKILL.md`)
* **Observation**: Fully documents actor network replication (`configure_actor_replication`) and variable replication/RepNotify callbacks (`set_variable_replication`).

#### 7. Skill: `niagara-authoring` (`UnrealEngine/skills/niagara-authoring/SKILL.md`)
* **Observation**: Fully documents Niagara System parameter overrides (`set_niagara_parameter`) for scalar, color, and `CurveFloat` parameter stores.

---

## Section 2: R2 — Developer Utility Scripts Refactoring (`UnrealEngine/src/scripts/`)

The 4 standalone developer scripts currently depend on the embedded Python `unreal` module (`unreal.EditorAssetLibrary`, `unreal.AssetRegistryHelpers`). To enable standalone execution from any Python environment (without requiring Unreal's embedded Python interpreter), these scripts must be refactored to communicate directly with the C++ HTTP Editor Server on port `18777` (`http://127.0.0.1:18777/api/execute_tool`) using standard Python `urllib.request`.

---

### 2.1 Refactored Script 1: `bulk_replace_references.py`

* **Original Dependency**: `unreal.EditorAssetLibrary.consolidate_assets`
* **Native C++ Tool Target**: `consolidate_asset_references` (`FAgentFrameworkContextActions`)
* **Refactored Source Code**:

```python
import json
import urllib.request
import urllib.error

EDITOR_HTTP_URL = "http://127.0.0.1:18777/api/execute_tool"

def bulk_replace_references(source_path, target_path):
    """
    Consolidates assets by replacing all references to source_path with target_path,
    then deletes source_path via the native C++ MCP tool 'consolidate_asset_references'.
    """
    if not source_path or not target_path:
        print("Error: Both source_path and target_path must be specified.")
        return False

    if source_path == target_path:
        print("Warning: Source and target paths are identical. Skipping.")
        return True

    payload = {
        "tool_name": "consolidate_asset_references",
        "parameters": {
            "source_asset_path": source_path,
            "target_asset_path": target_path
        }
    }

    req = urllib.request.Request(
        EDITOR_HTTP_URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"}
    )

    try:
        with urllib.request.urlopen(req, timeout=30) as response:
            res_data = json.loads(response.read().decode("utf-8"))
            if res_data.get("bSuccess"):
                print(f"Success: {res_data.get('ResultMessage')}")
                return True
            else:
                print(f"Error: {res_data.get('Errors')}")
                return False
    except urllib.error.URLError as e:
        print(f"Failed to connect to Editor HTTP server on port 18777: {e}")
        return False

if __name__ == "__main__":
    # Example usage:
    # bulk_replace_references("/Game/OldMat", "/Game/NewMat")
    pass
```

---

### 2.2 Refactored Script 2: `clean_naming_conventions.py`

* **Original Dependency**: `unreal.EditorAssetLibrary.list_assets`, `unreal.EditorAssetLibrary.rename_asset`
* **Native C++ Tool Target**: `enforce_naming_conventions` (`FAgentFrameworkContextActions`)
* **Refactored Source Code**:

```python
import json
import urllib.request
import urllib.error

EDITOR_HTTP_URL = "http://127.0.0.1:18777/api/execute_tool"

def clean_naming_conventions(folder_path, recursive=True, dry_run=False):
    """
    Scans folder_path and enforces UE5 asset naming conventions using the native C++ action tool.
    """
    payload = {
        "tool_name": "enforce_naming_conventions",
        "parameters": {
            "folder_path": folder_path,
            "recursive": recursive,
            "dry_run": dry_run
        }
    }

    req = urllib.request.Request(
        EDITOR_HTTP_URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"}
    )

    try:
        with urllib.request.urlopen(req, timeout=60) as response:
            res_data = json.loads(response.read().decode("utf-8"))
            if res_data.get("bSuccess"):
                renamed_count = res_data.get("renamed_assets_count", 0)
                print(f"Success: Enforced naming conventions on {folder_path}. Renamed: {renamed_count}")
                return True
            else:
                print(f"Error: {res_data.get('Errors')}")
                return False
    except urllib.error.URLError as e:
        print(f"Failed to connect to Editor HTTP server on port 18777: {e}")
        return False

if __name__ == "__main__":
    # Example usage:
    # clean_naming_conventions("/Game/TestFolder")
    pass
```

---

### 2.3 Refactored Script 3: `find_unreferenced_assets.py`

* **Original Dependency**: `unreal.AssetRegistryHelpers.get_asset_registry().get_referencers()`
* **Native C++ Tool Target**: `find_unreferenced_assets` (`FAgentFrameworkDiagnosticsActions`)
* **Refactored Source Code**:

```python
import json
import urllib.request
import urllib.error

EDITOR_HTTP_URL = "http://127.0.0.1:18777/api/execute_tool"

def find_unreferenced_assets(folder_path, include_soft_references=True):
    """
    Scans folder_path for unreferenced assets via native C++ AssetRegistry queries.
    """
    payload = {
        "tool_name": "find_unreferenced_assets",
        "parameters": {
            "folder_path": folder_path,
            "include_soft_references": include_soft_references
        }
    }

    req = urllib.request.Request(
        EDITOR_HTTP_URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"}
    )

    try:
        with urllib.request.urlopen(req, timeout=60) as response:
            res_data = json.loads(response.read().decode("utf-8"))
            if res_data.get("bSuccess"):
                unreferenced = res_data.get("unreferenced_assets", [])
                print(f"Found {len(unreferenced)} unreferenced assets in {folder_path}:")
                for asset in unreferenced:
                    print(f"  - {asset}")
                return unreferenced
            else:
                print(f"Error: {res_data.get('Errors')}")
                return []
    except urllib.error.URLError as e:
        print(f"Failed to connect to Editor HTTP server on port 18777: {e}")
        return []

if __name__ == "__main__":
    # Example usage:
    # find_unreferenced_assets("/Game/TestFolder")
    pass
```

---

### 2.4 Refactored Script 4: `organize_assets_by_type.py`

* **Original Dependency**: `unreal.EditorAssetLibrary.find_asset_data`, `unreal.EditorAssetLibrary.rename_asset`
* **Native C++ Tool Target**: `organize_assets_by_type` (`FAgentFrameworkContextActions`)
* **Refactored Source Code**:

```python
import json
import urllib.request
import urllib.error

EDITOR_HTTP_URL = "http://127.0.0.1:18777/api/execute_tool"

def organize_assets_by_type(folder_path, recursive=True):
    """
    Organizes assets in folder_path recursively into type-specific subfolders using native C++ action tool.
    """
    payload = {
        "tool_name": "organize_assets_by_type",
        "parameters": {
            "folder_path": folder_path,
            "recursive": recursive
        }
    }

    req = urllib.request.Request(
        EDITOR_HTTP_URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"}
    )

    try:
        with urllib.request.urlopen(req, timeout=60) as response:
            res_data = json.loads(response.read().decode("utf-8"))
            if res_data.get("bSuccess"):
                moved_count = res_data.get("moved_assets_count", 0)
                print(f"Success: Organized {folder_path}. Moved {moved_count} assets.")
                return True
            else:
                print(f"Error: {res_data.get('Errors')}")
                return False
    except urllib.error.URLError as e:
        print(f"Failed to connect to Editor HTTP server on port 18777: {e}")
        return False

if __name__ == "__main__":
    # Example usage:
    # organize_assets_by_type("/Game/TestFolder")
    pass
```

---

## Section 3: R3 — Test Suite & Test Runner Migration (`Tests/`)

### 3.1 Migration Strategy for `test_e2e_integration.py`

To ensure full test coverage of all native C++ tool routes implemented across Tiers 1-3, `test_e2e_integration.py` and dedicated test files (`test_m2_native_actions.py`, `test_m4_challenger2_context_actions.py`) must exercise each C++ tool route.

#### Complete Test Suite Mapping (18 Native Action Tools):

| # | Native C++ Tool Route | Test File | Target Test Function | Validation Assertions |
|---|---|---|---|---|
| 1 | `disconnect_blueprint_pins` | `test_e2e_integration.py` | `test_cpp_mcp_disconnect_blueprint_pins` | Verify `bSuccess=True` and `DisconnectedLinksCount >= 1` |
| 2 | `modify_blueprint_subobject` | `test_e2e_integration.py` | `test_cpp_mcp_modify_blueprint_subobject` | Verify sub-object mutation on nested path returns `bSuccess=True` |
| 3 | `configure_actor_replication` | `test_e2e_integration.py` | `test_cpp_mcp_configure_actor_replication` | Verify CDO flags `bReplicates` and `bReplicateMovement` set to `True` |
| 4 | `set_variable_replication` | `test_e2e_integration.py` | `test_cpp_mcp_set_variable_replication` | Verify `ReplicationType="RepNotify"` creates RepNotify callback |
| 5 | `configure_input_mapping_modifiers_triggers` | `test_e2e_integration.py` | `test_cpp_mcp_configure_input_mapping_modifiers_triggers` | Verify `AppliedModifiersCount >= 1` for `Negate`/`SwizzleAxis` |
| 6 | `set_niagara_parameter` | `test_m2_niagara_parameter_verification.py` | `test_cpp_mcp_set_niagara_parameter` | Verify `BoundDataType` matches requested parameter scope & type |
| 7 | `create_pbr_material_from_textures` | `test_e2e_integration.py` | `test_cpp_mcp_create_pbr_material_from_textures` | Verify `ExpressionsCreatedCount >= 2` and material compiles |
| 8 | `configure_sound_wave_cue` | `test_e2e_integration.py` | `test_cpp_mcp_configure_sound_wave_cue` | Verify `USoundCue` created with specified volume/pitch multiplier |
| 9 | `create_metasound_source` | `test_e2e_integration.py` | `test_cpp_mcp_create_metasound_source` | Verify `OutputFormat="Stereo"` asset created successfully |
| 10 | `wire_metasound_nodes` | `test_e2e_integration.py` | `test_cpp_mcp_wire_metasound_nodes` | Verify `NodesAddedCount >= 1` and `ConnectionsWiredCount >= 1` |
| 11 | `consolidate_asset_references` | `test_m2_native_actions.py` | `test_consolidate_asset_references_schema_validation` | Verify parameter validation and asset reference replacement |
| 12 | `enforce_naming_conventions` | `test_m4_challenger2_context_actions.py` | `test_parameter_aliasing_payload_formatting` | Verify payload aliasing and asset prefix enforcement |
| 13 | `find_unreferenced_assets` | `test_m2_native_actions.py` | `test_find_unreferenced_assets_schema_validation` | Verify unreferenced asset array returned for target folder |
| 14 | `organize_assets_by_type` | `test_m4_challenger2_context_actions.py` | `test_category_mappings_defined` | Verify asset moving into category folders (`Blueprints/`, etc.) |
| 15 | `inspect_uobject_properties` | `test_m2_native_actions.py` | `test_inspect_uobject_properties_schema_validation` | Verify live `UObject` property reflection without Python fallback |
| 16 | `set_widget_slot_properties` | `test_m3_challenger2_slot_properties.py` | `test_set_widget_slot_properties` | Verify UMG child widget canvas slot anchors and offsets updated |
| 17 | `invoke_pie_widget_delegate` | `test_e2e_integration.py` | `test_cpp_mcp_invoke_pie_widget_delegate` | Verify multicast delegate (`OnClicked`) broadcast on PIE widget |
| 18 | `add_blueprint_component` | `test_e2e_integration.py` | `test_cpp_mcp_add_blueprint_component` | Verify SCS component node attached to Blueprint asset |

---

### 3.2 Test Suite Execution Protocol (`run_all_tests.ps1`)

The consolidated test runner `run_all_tests.ps1` executes both test phases sequentially:
1. **C++ Headless Automation Tests**: Launches `UnrealEditor-Cmd.exe` with `-ExecCmds="Automation RunTests AgentFramework; Quit"` to validate in-editor C++ action executors headlessly.
2. **Python Integration Tests**: Launches pytest runner `run_tests.py` via explicit Python 3.13 interpreter to execute all 22 test files in `Tests/`.

Command to run full suite:
```powershell
powershell -ExecutionPolicy Bypass -File .\Tests\run_all_tests.ps1
```

---

## Section 4: Actionable Migration Checklist

- [x] **R1 (Skill Migration)**:
  - Add `disconnect_blueprint_pins` pin disconnection documentation to `blueprint-authoring/SKILL.md`.
  - Validate native C++ MCP tool routes across all 7 target skills (`blueprint-authoring`, `unreal-testing-sops`, `add-component`, `generate-assets`, `setup-input`, `setup-replication`, `niagara-authoring`).
- [ ] **R2 (Script Refactoring)**:
  - Replace `import unreal` in `bulk_replace_references.py` with HTTP client call to `consolidate_asset_references`.
  - Replace `import unreal` in `clean_naming_conventions.py` with HTTP client call to `enforce_naming_conventions`.
  - Replace `import unreal` in `find_unreferenced_assets.py` with HTTP client call to `find_unreferenced_assets`.
  - Replace `import unreal` in `organize_assets_by_type.py` with HTTP client call to `organize_assets_by_type`.
- [ ] **R3 (Test Suite Execution)**:
  - Execute `powershell -File .\Tests\run_tests.ps1` and `run_all_tests.ps1` to confirm 100% pass rates across native C++ tool routes.
