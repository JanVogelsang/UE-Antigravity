# Milestone 1: Skill Documentation Migration Analysis Report

## Executive Summary
This report provides the detailed analysis and step-by-step editing instructions for **Milestone 1: Skill Documentation Migration** of the `UE-AgentFramework` project.
The objective of Milestone 1 is to remove legacy Python script fallbacks (`execute_python_script` / `unreal.*` library calls) from AI agent skills and replace them with native C++ MCP tool routes documented in `Documentation/PYTHON_FALLBACK_AUDIT.md`.

This audit covers three specific target skills:
1. `UnrealEngine/skills/blueprint-authoring/SKILL.md`
2. `UnrealEngine/skills/unreal-testing-sops/SKILL.md`
3. `UnrealEngine/skills/add-component/SKILL.md`

---

## 1. Skill 1: `blueprint-authoring/SKILL.md`

### 1.1 Current State Observation
* **File Path**: `UnrealEngine/skills/blueprint-authoring/SKILL.md`
* **Target Line Range**: Lines 23-35
* **Current Content**:
  ```markdown
  ## Python Sub-Object Bypassing (Design Time)
  In Unreal Python, internal blueprint sub-objects (like `WidgetTree` elements in UMG or added Components in a standard Blueprint) are often protected and cannot be accessed via standard property reflection (e.g., `bp.get_editor_property('WidgetTree')`).
  To modify these sub-objects via Python scripts in the editor, bypass this restriction by loading the sub-object directly from its path using colon notation (`AssetPath.AssetName:SubObjectName`):
  ```python
  import unreal
  # Load the sub-object directly
  widget_obj = unreal.load_object(None, '/Game/UI/Path/W_MyWidget.W_MyWidget:WidgetTree.SubWidgetName')
  if widget_obj:
      slot = widget_obj.slot  # Access the layout slot (e.g., CanvasPanelSlot)
      slot.set_z_order(-1)
      slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0), maximum=unreal.Vector2D(1,1)))
  ```
  Always follow this pattern instead of attempting complex reflection hacks.
  ```

### 1.2 Audit Cross-Reference & Problem Statement
* **Audit Document**: `Documentation/PYTHON_FALLBACK_AUDIT.md` (Section 1.2 Fallback 1; Section 4 Specification 2 & Specification 16).
* **Problem**: Standard Blueprint reflection tools only operate on the top-level Class Default Object (CDO). Previously, agents relied on Python `unreal.load_object` with colon path notation to access nested sub-objects (SCS sub-components or `WidgetTree` elements) and adjust UMG slot layout parameters (`slot.set_anchors`, `slot.set_z_order`).
* **Native Replacements**:
  1. `modify_blueprint_subobject` (Specification 2): Mutates sub-object properties directly using colon/dot notation.
  2. `set_widget_slot_properties` (Specification 16): Mutates UMG slot layout properties (anchors, alignment, offsets, z-order) natively in C++.

### 1.3 Exact Step-by-Step Replacement Instructions

#### Action
Replace Lines 23–35 in `UnrealEngine/skills/blueprint-authoring/SKILL.md` with the native C++ tool documentation below.

#### Target Content (Lines 23–35 to remove)
```markdown
## Python Sub-Object Bypassing (Design Time)
In Unreal Python, internal blueprint sub-objects (like `WidgetTree` elements in UMG or added Components in a standard Blueprint) are often protected and cannot be accessed via standard property reflection (e.g., `bp.get_editor_property('WidgetTree')`).
To modify these sub-objects via Python scripts in the editor, bypass this restriction by loading the sub-object directly from its path using colon notation (`AssetPath.AssetName:SubObjectName`):
```python
import unreal
# Load the sub-object directly
widget_obj = unreal.load_object(None, '/Game/UI/Path/W_MyWidget.W_MyWidget:WidgetTree.SubWidgetName')
if widget_obj:
    slot = widget_obj.slot  # Access the layout slot (e.g., CanvasPanelSlot)
    slot.set_z_order(-1)
    slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0), maximum=unreal.Vector2D(1,1)))
```
Always follow this pattern instead of attempting complex reflection hacks.
```

#### Replacement Content
```markdown
## Design-Time Sub-Object & UMG Slot Property Modification (Native C++ Tools)

To modify internal blueprint sub-objects (such as nested sub-components or UMG `WidgetTree` child elements) at design time, use native C++ action routes instead of Python script execution.

### 1. General Sub-Object Mutation (`modify_blueprint_subobject`)
Use `modify_blueprint_subobject` to mutate property values on nested sub-objects using colon or dot path notation (`WidgetTree.SubWidgetName` or `SCS_Node.ComponentName`):

```json
{
  "AssetPath": "/Game/UI/W_MyWidget",
  "SubObjectPath": "WidgetTree.SubWidgetName",
  "Properties": {
    "bIsEnabled": false,
    "Visibility": "Collapsed"
  }
}
```

### 2. UMG Widget Layout & Slot Properties (`set_widget_slot_properties`)
To adjust slot layout properties (anchors, alignment, offsets, Z-order) on child widgets inside a UMG Widget Blueprint, call `set_widget_slot_properties`:

```json
{
  "widget_blueprint_path": "/Game/UI/W_MyWidget",
  "widget_name": "SubWidgetName",
  "anchors": { "min_x": 0.0, "min_y": 0.0, "max_x": 1.0, "max_y": 1.0 },
  "alignment": { "x": 0.5, "y": 0.5 },
  "offsets": { "left": 0.0, "top": 0.0, "right": 100.0, "bottom": 50.0 }
}
```
```

---

## 2. Skill 2: `unreal-testing-sops/SKILL.md`

### 2.1 Current State Observation
* **File Path**: `UnrealEngine/skills/unreal-testing-sops/SKILL.md`
* **Target Line Range**: Lines 63-111
* **Current Content**:
  ```markdown
  #### Option C: Programmatic UI Actions (Native MCP & Python Fallbacks)
  Use this option when input action bindings or console command cheats are not available for UI transitions.

  1. **Start PIE Session**:
     - Tool: `start_pie_session`
  2. **Navigate UI programmatically using Native MCP Tools (Recommended)**:
     - **Extract the active UI Tree**:
       - Tool: `extract_ui_state`
       - *Reasoning*: This returns a JSON hierarchy of all visible UMG widgets and Slate elements, along with their names (e.g., `W_TauMainMenu_C_0.PvPButton`).
     - **Trigger the Button Click**:
       - Tool: `trigger_ui_element` -> `widget_path`: `"W_TauMainMenu_C_0.PvPButton"` (using the name retrieved from `extract_ui_state`).
  3. **Python Scripting Fallback (If native tools are not available)**:
     - If utilizing a custom Python execution tool, get the active running PIE game world via the `UnrealEditorSubsystem` (since `EditorLevelLibrary.get_editor_world()` retrieves the static editor world, which does not contain PIE runtime widgets):
       ```python
       import unreal
       # Get the active PIE game world
       editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
       game_world = editor_subsystem.get_game_world()
       
       # Query widgets in the PIE world
       widgets = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(game_world, unreal.UserWidget, True)
       for w in widgets:
           if w.get_name() == "W_TauMainMenu_C" or "MainMenu" in w.get_name():
               pvp_button = w.get_editor_property("PvPButton")
               if pvp_button:
                   pvp_button.on_clicked.broadcast()
                   print("PvP button clicked programmatically")
                   break
       ```
  4. **Wait for Loading**:
     - Pause execution for 2-3 seconds.
  5. **Navigate/Click next screen programmatically**:
     - Use the native `extract_ui_state` and `trigger_ui_element` on the newly loaded `W_FleetManagementUI` widgets.
     - Or use the Python fallback in the game world context:
       ```python
       import unreal
       editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
       game_world = editor_subsystem.get_game_world()
       
       widgets = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(game_world, unreal.UserWidget, True)
       for w in widgets:
           if "FleetManagement" in w.get_name():
               start_button = w.get_editor_property("StartRoundButton")
               if start_button:
                   start_button.on_clicked.broadcast()
                   print("Start Round button clicked programmatically")
                   break
       ```
  ```

### 2.2 Audit Cross-Reference & Problem Statement
* **Audit Document**: `Documentation/PYTHON_FALLBACK_AUDIT.md` (Section 1.2 Fallback 2; Section 4 Specification 17).
* **Problem**: Native Slate hit-testing tools (`trigger_ui_element`) rely on Slate geometry and synthetic mouse event injection, which can fail if widgets are occluded, off-screen, or unfocused. Testing SOPs previously fell back to Python scripts using `unreal.WidgetBlueprintLibrary.get_all_widgets_of_class` to query PIE runtime widgets and broadcast multicast delegates (`on_clicked.broadcast()`).
* **Native Replacements**:
  1. `get_active_runtime_widgets` (Specification 17): Enumerates active runtime UMG widget instances in the PIE game world natively.
  2. `invoke_pie_widget_delegate` (Specification 17): Directly invokes multicast script delegates (such as `OnClicked`) on runtime UMG widgets during PIE sessions without relying on physical Slate mouse input.

### 2.3 Exact Step-by-Step Replacement Instructions

#### Action
Replace Lines 63–111 in `UnrealEngine/skills/unreal-testing-sops/SKILL.md` with the updated native C++ tool instructions below.

#### Target Content (Lines 63–111 to remove)
(Same as section 2.1)

#### Replacement Content
```markdown
#### Option C: Programmatic UI Actions (Native MCP C++ Tools)
Use this option when input action bindings or console command cheats are not available for UI transitions.

1. **Start PIE Session**:
   - Tool: `start_pie_session`
2. **Enumerate & Inspect Active PIE Widgets**:
   - Query active UMG widget instances in the running PIE game world:
     - Tool: `get_active_runtime_widgets` -> `widget_class`: `"UserWidget"`
   - Or extract the full visual Slate element hierarchy:
     - Tool: `extract_ui_state`
3. **Trigger UI Actions via Native MCP Tools**:
   - **Method A: Direct Delegate Invocation (Recommended for Off-Screen/Unfocused Widgets)**:
     - Directly trigger the button delegate on the active PIE widget instance:
       - Tool: `invoke_pie_widget_delegate` -> `widget_class_or_name`: `"W_TauMainMenu_C"`, `widget_property_name`: `"PvPButton"`, `delegate_name`: `"OnClicked"`
   - **Method B: Slate Event Simulation**:
     - Trigger synthesized click on the widget path retrieved from `extract_ui_state`:
       - Tool: `trigger_ui_element` -> `widget_path`: `"W_TauMainMenu_C_0.PvPButton"`
4. **Wait for Loading**:
   - Pause execution for 2-3 seconds.
5. **Navigate/Click next screen programmatically**:
   - Trigger the start round delegate on the newly loaded `W_FleetManagementUI` widget:
     - Tool: `invoke_pie_widget_delegate` -> `widget_class_or_name`: `"W_FleetManagementUI"`, `widget_property_name`: `"StartRoundButton"`, `delegate_name`: `"OnClicked"`
```

---

## 3. Skill 3: `add-component/SKILL.md`

### 3.1 Current State Observation
* **File Path**: `UnrealEngine/skills/add-component/SKILL.md`
* **Current Line Count**: 32 lines
* **Current Content**:
  Details C++ header declaration (`UPROPERTY`) and constructor component creation (`CreateDefaultSubobject`, `SetupAttachment`). Lacks any mention of design-time SCS component attachment to existing Blueprint `.uasset` files via MCP tools.

### 3.2 Audit Cross-Reference & Problem Statement
* **Audit Document**: `Documentation/PYTHON_FALLBACK_AUDIT.md` (Section 1.3 Gap 1; Section 4 Specification 18).
* **Problem**: When an AI agent needs to add a component to a Blueprint asset at design time without touching C++ source files, `add-component/SKILL.md` provided no tool guidance. Previously agents had to write custom C++ code or attempt complex batch operations.
* **Native Replacement**:
  - `add_blueprint_component` (Specification 18): Adds and attaches Simple Construction Script (SCS) component nodes directly to Blueprint assets at design time.

### 3.3 Exact Step-by-Step Replacement Instructions

#### Action
Append a new section `## Design-Time Blueprint Component Attachment (Native C++ Tool)` to the end of `UnrealEngine/skills/add-component/SKILL.md` (after line 32).

#### Target Insertion Location
After Line 32 of `UnrealEngine/skills/add-component/SKILL.md`.

#### Content to Append
```markdown

---

## Design-Time Blueprint Component Attachment (Native C++ Tool)

To attach a new component to an existing Blueprint asset (`.uasset`) at design time without modifying C++ source code, use the native C++ action tool `add_blueprint_component`.

### Usage Protocol
Invoke `add_blueprint_component` with the target Blueprint asset path, component class, new component name, and optional parent attachment node:

```json
{
  "blueprint_path": "/Game/Blueprints/BP_MyActor",
  "component_class": "UStaticMeshComponent",
  "component_name": "MeshComponent",
  "parent_component_name": "DefaultSceneRoot"
}
```

### Parameters
* `blueprint_path` (string, required): Object path to target Blueprint asset.
* `component_class` (string, required): Component class name (e.g. `'UStaticMeshComponent'`, `'USphereComponent'`, `'UAudioComponent'`, `'UNiagaraComponent'`).
* `component_name` (string, required): Unique identifier name for the new SCS component node.
* `parent_component_name` (string, optional): Parent component node to attach under. Defaults to `RootComponent` if omitted.
```

---

## Summary Matrix of Skill Updates

| Skill File | Removed Python Snippet | Added Native C++ Tools | Audit Reference |
|---|---|---|---|
| `blueprint-authoring/SKILL.md` | `unreal.load_object` for sub-object layout/anchors | `modify_blueprint_subobject`, `set_widget_slot_properties` | Specs 2 & 16 |
| `unreal-testing-sops/SKILL.md` | `unreal.WidgetBlueprintLibrary.get_all_widgets_of_class` & delegate broadcast | `invoke_pie_widget_delegate`, `get_active_runtime_widgets` | Spec 17 |
| `add-component/SKILL.md` | None (was C++ pattern only) | `add_blueprint_component` | Spec 18 |
