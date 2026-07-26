# Skills Audit & Native C++ Action API Specifications (Phase 1 Audit Report)

**Repository**: `UE-Antigravity` (`c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity`)  
**Audited Directory**: `UnrealEngine/skills/` (and `.agents/skills/` / target project skills)  
**Date**: July 26, 2026  
**Auditor**: Explorer Subagent (`teamwork_preview_explorer_m1_1`)  

---

## Executive Summary

A comprehensive audit of all **14 skill folders** across `UnrealEngine/skills/` and the project workspace was conducted to identify any use or recommendation of `execute_python_script` or `unreal.*` Python module calls.

### Audit Summary Matrix

| # | Skill Name | Path / Location | Python Fallback Present? | Primary Purpose |
|---|---|---|---|---|
| 1 | `add-component` | `UnrealEngine/skills/add-component/SKILL.md` | ❌ No (C++ Code Pattern) | C++ component declaration & attachment |
| 2 | `blueprint-authoring` | `UnrealEngine/skills/blueprint-authoring/SKILL.md` | ⚠️ **YES** (`unreal.load_object`) | Blueprint modification & T3D graph injection |
| 3 | `create-actor` | `UnrealEngine/skills/create-actor/SKILL.md` | ❌ No | C++ Actor class creation boilerplate |
| 4 | `create-interface` | `UnrealEngine/skills/create-interface/SKILL.md` | ❌ No | C++ / Blueprint Interface boilerplate |
| 5 | `generate-assets` | `UnrealEngine/skills/generate-assets/SKILL.md` | ℹ️ CLI Python Script (External) | Third-party REST API generation (Meshy/ElevenLabs) |
| 6 | `niagara-authoring` | `UnrealEngine/skills/niagara-authoring/SKILL.md` | ❌ No | Native C++ Niagara VFX creation & rendering |
| 7 | `pie-verifier` | `UnrealEngine/skills/pie-verifier/SKILL.md` | ❌ No | Live PIE testing & UMG/Slate verification |
| 8 | `python-env` | `UnrealEngine/skills/python-env/SKILL.md` | ℹ️ Environment Setup Only | Pytest & MCP stdio IPC configuration |
| 9 | `setup-input` | `UnrealEngine/skills/setup-input/SKILL.md` | ❌ No | Enhanced Input bindings & setup |
| 10 | `setup-replication` | `UnrealEngine/skills/setup-replication/SKILL.md` | ❌ No | Network replication & RPC configuration |
| 11 | `unreal-instructions` | `UnrealEngine/skills/unreal-instructions/SKILL.md` | ❌ No | Mandatory Dual-MCP entry point & routing |
| 12 | `unreal-setup` | `UnrealEngine/skills/unreal-setup/SKILL.md` | ℹ️ CLI Pip Setup Only | Project setup & AST indexing |
| 13 | `unreal-testing-sops` | `UnrealEngine/skills/unreal-testing-sops/SKILL.md` | ⚠️ **YES** (`unreal.WidgetBlueprintLibrary`) | Testing SOPs & runtime PIE widget triggering |
| 14 | `project-index` | `.agents/skills/project-index/SKILL.md` | ❌ No | Architecture index for target host project |

---

## Detailed Fallback Analysis & Phase 2 Native C++ API Proposals

---

### Fallback 1: Design-Time Blueprint & UMG Internal Sub-Object Property Modification

- **Feature / Subsystem Name**: Design-Time Blueprint Sub-Object & UMG Layout Slot Property Mutation
- **Skill Name & File Path**: `blueprint-authoring` (`UnrealEngine/skills/blueprint-authoring/SKILL.md`)
- **Current Python Fallback Implementation Snippet**:
  ```python
  import unreal
  # Load the sub-object directly using colon notation
  widget_obj = unreal.load_object(None, '/Game/UI/Path/W_MyWidget.W_MyWidget:WidgetTree.SubWidgetName')
  if widget_obj:
      slot = widget_obj.slot  # Access the layout slot (e.g., CanvasPanelSlot)
      slot.set_z_order(-1)
      slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0), maximum=unreal.Vector2D(1,1)))
  ```
- **Reason Native C++ Actions are Currently Insufficient**:
  In Unreal Engine Python and Editor reflection, internal blueprint sub-objects (like `WidgetTree` child elements in UMG widgets or components attached via SCS in standard Blueprints) are protected/private properties (`bp.get_editor_property('WidgetTree')` fails or returns unmodifiable pointers).
  Current native C++ MCP tools (`set_blueprint_property`, `set_widget_slot`) require top-level target specifiers and do not support sub-object path syntax (`AssetPath.AssetName:SubObjectName.ChildProperty`) to load, traverse, and mutate nested sub-objects directly.
- **Proposed Native C++ Action API Specification (Phase 2)**:
  - **Action Route Name**: `modify_blueprint_subobject`
  - **Target Action Module**: `AgentFrameworkBlueprintActions` / `AgentFrameworkWidgetActions`
  - **Category**: `Blueprint` / `UMG`
  - **Parameters Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "asset_path": {
          "type": "string",
          "description": "Long package path to the Blueprint asset (e.g. '/Game/UI/W_MyWidget')"
        },
        "subobject_path": {
          "type": "string",
          "description": "Relative sub-object path using colon/dot notation (e.g. 'WidgetTree.SubWidgetName' or 'SimpleConstructionScript.MyComponent')"
        },
        "property_name": {
          "type": "string",
          "description": "Property or slot property to set (e.g. 'Slot.ZOrder', 'Slot.Anchors', 'Visibility')"
        },
        "value": {
          "description": "JSON payload containing target value or struct members"
        }
      },
      "required": ["asset_path", "subobject_path", "property_name", "value"]
    }
    ```
  - **C++ Engine Implementation Approach**:
    1. Construct full object path: `FString FullPath = FString::Printf(TEXT("%s.%s:%s"), *AssetPath, *AssetName, *SubObjectPath);`
    2. Load object directly: `UObject* SubObj = StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath);`
    3. Traversal & Mutation: Resolve property via `FProperty* Prop = SubObj->GetClass()->FindPropertyByName(*PropertyName);` or handle `UPanelSlot` layout slots via `UWidget::Slot`.
    4. Serialization & Notification: Call `Prop->ImportText_Direct(*ValueStr, Prop->ContainerPtrToValuePtr<void>(SubObj), SubObj, PPF_None);` followed by `FBlueprintEditorUtils::MarkBlueprintAsModified(BP)`.

---

### Fallback 2: Runtime PIE UMG Widget Query & Delegate Triggering

- **Feature / Subsystem Name**: Live Play-In-Editor (PIE) UMG Widget Inspection & Direct Script Delegate Invocation
- **Skill Name & File Path**: `unreal-testing-sops` (`UnrealEngine/skills/unreal-testing-sops/SKILL.md`)
- **Current Python Fallback Implementation Snippet**:
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
- **Reason Native C++ Actions are Currently Insufficient**:
  The existing native `extract_ui_state` and `trigger_ui_element` MCP tools operate via Slate geometry matching and hit-testing (clicking screen coordinates). In runtime PIE testing:
  1. If a UMG widget is occluded, zero-sized, collapsed, or lacks a focused Slate widget handle, `trigger_ui_element` fails to trigger the click event.
  2. Current native C++ MCP actions do NOT provide a route to iterate runtime `UUserWidget` instances in the PIE `UWorld` and invoke multicast delegates (like `OnClicked`, `OnHovered`, `OnSelectionChanged`, `OnValueChanged`) directly on internal `UWidget` properties without UI mouse input events.
- **Proposed Native C++ Action API Specification (Phase 2)**:
  - **Action Route Name**: `invoke_pie_widget_delegate`
  - **Target Action Module**: `AgentFrameworkPIEActions`
  - **Category**: `PIE` / `Widget`
  - **Parameters Schema**:
    ```json
    {
      "type": "object",
      "properties": {
        "widget_class_or_name": {
          "type": "string",
          "description": "Class name or instance name of the target UUserWidget (e.g. 'W_TauMainMenu_C' or 'MainMenu')"
        },
        "widget_property_name": {
          "type": "string",
          "description": "Internal child widget member property name (e.g. 'PvPButton' or 'StartRoundButton')"
        },
        "delegate_name": {
          "type": "string",
          "default": "OnClicked",
          "description": "Multicast delegate property to broadcast (e.g. 'OnClicked', 'OnHovered')"
        }
      },
      "required": ["widget_class_or_name", "widget_property_name"]
    }
    ```
  - **C++ Engine Implementation Approach**:
    1. Obtain active PIE world context: `UWorld* PIEWorld = GEditor->GetPIEWorldContext()->World();`
    2. Query widgets: Call `UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PIEWorld, UUserWidget::StaticClass(), FoundWidgets, true);`
    3. Filter target widget instance matching `widget_class_or_name`.
    4. Access child property via reflection: `FProperty* ChildProp = UserWidget->GetClass()->FindPropertyByName(*WidgetPropertyName);`
    5. Resolve child `UWidget*` instance and locate delegate property: `FMulticastDelegateProperty* DelegateProp = CastField<FMulticastDelegateProperty>(ChildWidget->GetClass()->FindPropertyByName(*DelegateName));`
    6. Execute delegate broadcast: `FMulticastScriptDelegate* ScriptDelegate = DelegateProp->GetMulticastDelegate(ChildWidget); ScriptDelegate->ProcessMulticastDelegate(nullptr);`

---

### Additional Native C++ Action API Proposals (Addressing Tool Gaps)

In addition to the explicit Python script fallbacks, the audit identified two architectural tool gaps where agents must currently resort to multi-step boilerplate or manual C++ code editing:

#### Proposal 3: Atomic Design-Time Blueprint Component Attachment (`add_blueprint_component`)
- **Skill Reference**: `add-component` (`UnrealEngine/skills/add-component/SKILL.md`)
- **Gap Description**: While `add-component` provides C++ constructor code patterns, agents cannot attach a component to an existing Blueprint `.uasset` at design time via native C++ tools. `execute_batch_blueprint_operations` handles graph nodes, but not SimpleConstructionScript (SCS) component nodes.
- **Proposed Native C++ Action API**:
  - **Route Name**: `add_blueprint_component`
  - **Parameters**: `blueprint_path` (string), `component_class` (string), `component_name` (string), `parent_component_name` (optional string).
  - **Implementation**: Access `BP->SimpleConstructionScript`, call `USimpleConstructionScript::CreateNode`, attach to root or parent SCS node, and execute `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP)`.

#### Proposal 4: Atomic PBR Material Auto-Wiring (`create_pbr_material`)
- **Skill Reference**: `generate-assets` (`UnrealEngine/skills/generate-assets/SKILL.md`)
- **Gap Description**: Creating a PBR Material asset currently requires 6 sequential tool calls (`import_mesh`, `import_assets_batch`, `create_material`, `add_material_expression` x4, `connect_material_expression` x4, `assign_material`), creating unnecessary token overhead and potential failure points.
- **Proposed Native C++ Action API**:
  - **Route Name**: `create_pbr_material`
  - **Parameters**: `material_path` (string), `base_color_texture` (string), `normal_texture` (string), `roughness_texture` (string), `metallic_texture` (string), `target_mesh_path` (optional string).
  - **Implementation**: Wrap `UMaterialEditingLibrary` functions into an atomic C++ action that instantiates `UMaterialExpressionTextureSample` nodes, connects expressions to standard material inputs (`MP_BaseColor`, `MP_Normal`, `MP_Roughness`, `MP_Metallic`), compiles the material, and assigns it to the static mesh if requested.

---

## Conclusion & Next Steps

1. **Native C++ Coverage**: 11 of 14 skills operate with 100% pure native C++ MCP tools or code patterns.
2. **Explicit Python Fallbacks**: Exactly **2 skills** (`blueprint-authoring` and `unreal-testing-sops`) contain explicit `unreal.*` Python fallbacks.
3. **Phase 2 Implementation Priority**:
   - Priority 1: Implement `modify_blueprint_subobject` in `AgentFrameworkBlueprintActions`.
   - Priority 2: Implement `invoke_pie_widget_delegate` in `AgentFrameworkPIEActions`.
   - Priority 3: Implement `add_blueprint_component` in `AgentFrameworkBlueprintActions`.
   - Priority 4: Implement `create_pbr_material` in `AgentFrameworkMaterialActions`.

Once Phase 2 implements these 4 native C++ Action APIs, all Python fallback instructions in `blueprint-authoring` and `unreal-testing-sops` can be replaced with pure native MCP tool calls.
