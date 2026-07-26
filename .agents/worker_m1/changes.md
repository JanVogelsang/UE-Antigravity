# Skill Documentation Migration Changes Log (Milestone 1)

This log records the modifications made to the 7 target skill files in `UnrealEngine/skills/` to remove legacy Python script fallbacks (`execute_python_script` / `unreal.*` calls) and document native C++ MCP tool routes.

## Summary of Changes

| Target Skill File | Summary of Edits Made | Replaced Legacy Python Fallbacks | Added Native C++ MCP Tools |
|---|---|---|---|
| `UnrealEngine/skills/blueprint-authoring/SKILL.md` | Replaced Section "Python Sub-Object Bypassing (Design Time)" with native C++ sub-object & UMG slot modification tools. | `unreal.load_object` colon notation for `WidgetTree`/slot access | `modify_blueprint_subobject`, `set_widget_slot_properties` |
| `UnrealEngine/skills/unreal-testing-sops/SKILL.md` | Replaced Option C Python scripting fallbacks for active PIE runtime widget lookup and delegate broadcasting with native C++ tools. | `unreal.WidgetBlueprintLibrary.get_all_widgets_of_class`, `on_clicked.broadcast()` | `get_active_runtime_widgets`, `invoke_pie_widget_delegate` |
| `UnrealEngine/skills/add-component/SKILL.md` | Appended design-time SCS component attachment section for attaching components directly to Blueprint assets without C++ code edits. | N/A (previously C++ code pattern only) | `add_blueprint_component` |
| `UnrealEngine/skills/generate-assets/SKILL.md` | Replaced multi-step material expression tool churn in Step 3 and added audio sound wave/cue setup in Step 4. | Multi-step expression tool churn (`create_material` -> `add_material_expression` -> `connect_material_expression`) | `create_pbr_material_from_textures`, `configure_sound_wave_cue` |
| `UnrealEngine/skills/setup-input/SKILL.md` | Replaced abstract input action steps with concrete native MCP tool calls for input action/context creation and key mapping modifiers/triggers. | N/A (previously abstract text instructions) | `create_input_action`, `create_input_mapping_context`, `add_input_mapping`, `configure_input_mapping_modifiers_triggers` |
| `UnrealEngine/skills/setup-replication/SKILL.md` | Updated steps to provide Option A (Blueprint native tool routes for actor and variable network replication) alongside Option B (C++ patterns). | N/A (previously C++ code pattern only) | `configure_actor_replication`, `set_variable_replication` |
| `UnrealEngine/skills/niagara-authoring/SKILL.md` | Updated Step 4 and Step 6 to document System/Emitter user parameters and keyframe curve overrides. | Limited parameter binding instructions | `set_niagara_parameter` |

---

## Detailed File Modifications

### 1. `UnrealEngine/skills/blueprint-authoring/SKILL.md`
- **Lines Modified**: Lines 23-35 replaced with lines 23-64.
- **Removed**: Legacy Python sub-object loading via `unreal.load_object(None, '/Game/UI/Path/W_MyWidget.W_MyWidget:WidgetTree.SubWidgetName')` and layout slot modification.
- **Added**:
  - `modify_blueprint_subobject`: Enables mutating properties on sub-objects via `WidgetTree.SubWidgetName` or `SCS_Node.ComponentName`.
  - `set_widget_slot_properties`: Enables configuring anchors, alignment, offsets, and Z-order on child UMG widgets natively in C++.

### 2. `UnrealEngine/skills/unreal-testing-sops/SKILL.md`
- **Lines Modified**: Lines 63-111 replaced with lines 63-86.
- **Removed**: Legacy Python scripts searching for PIE widgets via `unreal.WidgetBlueprintLibrary.get_all_widgets_of_class` and triggering `on_clicked.broadcast()`.
- **Added**:
  - `get_active_runtime_widgets`: Enables querying active UMG widget instances in running PIE game world.
  - `invoke_pie_widget_delegate`: Enables directly invoking multicast script delegates (such as `OnClicked`) on active PIE widgets without physical Slate mouse input.

### 3. `UnrealEngine/skills/add-component/SKILL.md`
- **Lines Modified**: Appended section at the end of the file.
- **Added**:
  - `add_blueprint_component`: Enables attaching SCS component nodes (`UStaticMeshComponent`, `USphereComponent`, etc.) directly to Blueprint assets at design time.

### 4. `UnrealEngine/skills/generate-assets/SKILL.md`
- **Lines Modified**: Steps 2-3 updated, Step 4 added (Lines 39-84).
- **Removed**: 6+ sequential tool call churn (`create_material`, `add_material_expression`, `connect_material_expression` for each texture).
- **Added**:
  - `create_pbr_material_from_textures`: Atomic creation of complete PBR material graphs from texture maps.
  - `configure_sound_wave_cue`: Setting playback parameters (`bLooping`, `VolumeMultiplier`, `PitchMultiplier`) and instantiating 3D `USoundCue` assets.

### 5. `UnrealEngine/skills/setup-input/SKILL.md`
- **Lines Modified**: Rewritten with full C++ MCP tool specifications.
- **Added**:
  - `create_input_action`: Creating `UInputAction` assets with specified `ValueType`.
  - `create_input_mapping_context`: Creating `UInputMappingContext` assets.
  - `add_input_mapping`: Binding key mappings to actions.
  - `configure_input_mapping_modifiers_triggers`: Attaching `Modifiers` (`Negate`, `SwizzleAxis`) and `Triggers` (`Pressed`, `Hold`, `Tap`, `Pulse`).

### 6. `UnrealEngine/skills/setup-replication/SKILL.md`
- **Lines Modified**: Steps section updated to include Option A and Option B.
- **Added**:
  - `configure_actor_replication`: Setting network replication defaults (`bReplicates`, `bReplicateMovement`, `NetDormancy`, `NetUpdateFrequency`, `NetPriority`) on actor CDOs.
  - `set_variable_replication`: Setting variable replication type (`Replicated`, `RepNotify`), auto-generating RepNotify callbacks (`RepNotifyFunc`), and configuring lifetime conditions (`COND_OwnerOnly`, `COND_SkipOwner`).

### 7. `UnrealEngine/skills/niagara-authoring/SKILL.md`
- **Lines Modified**: Step 4 and Step 6 updated.
- **Added**:
  - `set_niagara_parameter`: Setting User/System/Emitter parameters (`User.SpawnRate`, `User.PrimaryColor`) and dynamic float/color curve keyframe arrays (`CurveFloat`) via `UNiagaraUserRedirectionParameterStore`.

---

## Verification Results
- Executed `grep_search` across `UnrealEngine/skills/` for `execute_python_script` -> **0 matches**.
- Executed `grep_search` across `UnrealEngine/skills/` for `unreal.` -> **0 matches**.
- Verified all 7 target skill files render clean markdown with exact JSON payloads matching C++ MCP action routes.
