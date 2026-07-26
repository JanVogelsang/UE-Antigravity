# UE-AgentFramework — Plugin Improvement Roadmap & Action Plan

This document outlines the strategic roadmap for expanding native C++ tool routes in `UE-AgentFramework`. It prioritizes auditing the entire codebase (skills, scripts, test cases, and guidelines) to identify every feature currently relying on `execute_python_script` fallbacks due to missing or non-granular C++ action support, followed by implementing native C++ replacements.

---

## Phase 1: Comprehensive Python Fallback & Feature Gap Audit

The primary objective of Phase 1 is to scan the entire repository (`skills/`, `UnrealEngine/`, `Tests/`, `Documentation/`) for all instances where agents or scripts currently use `execute_python_script` or `unreal` Python module calls because native C++ action routes in `AgentFrameworkActions` are either missing or lack granular node/property manipulation.

* [x] **Codebase & Skills Audit for Python Fallbacks**:
  * Scanned all 14 skills in `UnrealEngine/skills/`, test scripts in `Tests/`, developer scripts in `src/scripts/`, and all 183 native C++ action tools across 27 modules (`AgentFrameworkActions`).
  * Identified 18 concrete capability gaps and Python fallback scenarios (such as MetaSound node graph building, Blueprint pin disconnection, sub-object mutation, and Enhanced Input modifiers/triggers).
* [x] **Compilation of Findings into `PYTHON_FALLBACK_AUDIT.md`**:
  * Published comprehensive audit report saved at [PYTHON_FALLBACK_AUDIT.md](file:///c:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/Documentation/PYTHON_FALLBACK_AUDIT.md) (1,334 lines, 73.9 KB).
  * Formulated 18 complete Native C++ Action API specifications with JSON request/response schemas, C++ execution plans, and a 3-tier prioritized Phase 2 implementation roadmap.

---

## Phase 2: Implementation of Native C++ Action Routes

Phase 2 executes targeted C++ implementations in `AgentFrameworkActions` to replace every cataloged fallback scenario in [PYTHON_FALLBACK_AUDIT.md](file:///c:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/Documentation/PYTHON_FALLBACK_AUDIT.md) with dedicated native C++ tool routes across 3 prioritized tiers:

### Tier 1: Core Blueprint, Material & Audio Foundation (High Priority)
* [ ] **`disconnect_blueprint_pins`** (`Blueprint` module): Granular pin disconnection using `UEdGraphPin::BreakLinkTo`.
* [ ] **`modify_blueprint_subobject`** (`Blueprint` module): Design-time sub-object mutation via colon object paths.
* [ ] **`configure_actor_replication`** (`Blueprint` module): Direct CDO replication flag configuration (`bReplicates`, `bAlwaysRelevant`).
* [ ] **`set_variable_replication`** (`Blueprint` module): Blueprint variable replication & RepNotify function assignment.
* [ ] **`create_pbr_material_from_textures`** (`Material` module): One-shot PBR material graph generation & texture parameter wiring.
* [ ] **`create_metasound_source`** (`MetaSound` module - New): Native MetaSound asset creation using `IMetasoundFrontendDocumentBuilder`.
* [ ] **`wire_metasound_nodes`** (`MetaSound` module - New): Granular MetaSound graph node insertion & pin connection.

### Tier 2: Subsystem & Asset Workflow Enhancements (Medium Priority)
* [ ] **`configure_input_mapping_modifiers_triggers`** (`Input` module): Enhanced Input modifier & trigger binding.
* [ ] **`set_niagara_parameter`** (`Niagara` module): Niagara User Parameter override & curve assignment.
* [ ] **`configure_sound_wave_cue`** (`Media` module): SoundWave attenuation & SoundCue node graph authoring.
* [ ] **`inspect_uobject_properties`** (`Diagnostics` module): Live UObject property reflection & property tree inspection.
* [ ] **`set_widget_slot_properties`** (`Widget` module): UMG sub-widget slot layout & anchor mutation.
* [ ] **`invoke_pie_widget_delegate`** (`PIE` module): Runtime PIE widget event delegate broadcast & click triggering.
* [ ] **`get_active_runtime_widgets`** (`Widget`/`PIE` module): Live runtime Slate/UMG widget enumeration & state inspection.

### Tier 3: Developer Utility & Bulk Asset Automation (Lower Priority)
* [ ] **`consolidate_asset_references`** (`Context`/`Asset` module): Bulk asset reference replacement & consolidation.
* [ ] **`enforce_naming_conventions`** (`Context` module): Batch asset renaming according to naming standard rubrics.
* [ ] **`find_unreferenced_assets`** (`Diagnostics` module): Project-wide unreferenced asset auditing via AssetRegistry.
* [ ] **`organize_assets_by_type`** (`Context` module): Automated asset directory restructuring by UClass type.

---

## Phase 3: Skill & Test Suite Migration

* [ ] Update corresponding `SKILL.md` documents and integration test cases in `Tests/` to use the newly created native C++ tools instead of `execute_python_script`.
* [ ] Execute `powershell -File .\Tests\run_tests.ps1` to verify clean compilation via UBT and 100% test pass rates.
