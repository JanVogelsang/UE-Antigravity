# C++ Engine API Audit: UMG `UWidget` & `UPanelSlot` Property Manipulation (Milestone 3, Spec 16)

## 1. Executive Summary & Objective

This report presents a comprehensive technical audit of Unreal Engine 5 UMG C++ APIs for `UWidget` and `UPanelSlot` property manipulation, specifically targeting the implementation of Spec 16 (`set_widget_slot_properties`) inside `FAgentFrameworkWidgetActions` (`AgentFramework/Source/AgentFrameworkActions/Private/Widget/AgentFrameworkWidgetActions.cpp` and `Public/Widget/AgentFrameworkWidgetActions.h`).

The goal of `set_widget_slot_properties` is to provide native C++ execution for configuring sub-widget layout slots (`UCanvasPanelSlot`, `UVerticalBoxSlot`, `UHorizontalBoxSlot`, `UOverlaySlot`, `UGridSlot`, `UUniformGridSlot`, `UScrollBoxSlot`, `UWrapBoxSlot`, `UWidgetSwitcherSlot`, `USizeBoxSlot`, `UScaleBoxSlot`, `UBorderSlot`, etc.), eliminating Python script fallback workarounds (`unreal.load_object` + slot mutation).

---

## 2. UMG Object Architecture & Slot Lifecycle

### 2.1 Object Relationship
In Unreal Engine UMG:
- A `UWidgetBlueprint` owns a `UWidgetTree` (`WidgetBP->WidgetTree`).
- A `UWidgetTree` contains a hierarchy of `UWidget` instances (`UPanelWidget` container nodes and leaf nodes like `UTextBlock`, `UButton`, `UImage`).
- Every `UWidget` contains a public pointer property: `UPanelSlot* Slot`.

```
UWidgetBlueprint
  └── WidgetTree (UWidgetTree*)
        └── RootWidget (UWidget*)
              └── [PanelWidget (e.g. UCanvasPanel)]
                    └── Children (UWidget*)
                          └── Slot (UPanelSlot* -> Cast to UCanvasPanelSlot*, etc.)
```

### 2.2 Slot Lifecycle & Nullability
- **Root Widgets**: The root widget of a `WidgetTree` has **no slot** (`Widget->Slot == nullptr`), as it is not placed inside a parent container panel.
- **Unattached Widgets**: Widgets created via `UWidgetTree::ConstructWidget` that have not yet been added to a parent panel via `UPanelWidget::AddChild` have `Widget->Slot == nullptr`.
- **Slotted Widgets**: When a widget is added to a parent container (e.g., `ParentPanel->AddChild(ChildWidget)`), the parent panel instantiates a specific `UPanelSlot` subclass (e.g., `UCanvasPanelSlot`), sets `ChildWidget->Slot = NewSlot`, sets `NewSlot->Parent = ParentPanel`, and `NewSlot->Content = ChildWidget`.
- **Validation Requirement**: Before attempting any slot manipulation, code **must** check `IsValid(Widget->Slot)`. If null, an actionable error message must be returned indicating that the widget must first be attached to a parent panel via `add_widget`.

---

## 3. Detailed C++ API Audit by `UPanelSlot` Subclass

### 3.1 `UCanvasPanelSlot`
- **Container**: `UCanvasPanel`
- **Header**: `#include "Components/CanvasPanelSlot.h"`
- **Cast**: `UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);`
- **Key Structs**:
  - `FAnchors`: `Minimum` (`FVector2D`), `Maximum` (`FVector2D`). Predefined anchors exist in engine (e.g. Top-Left `(0,0,0,0)`, Top-Right `(1,0,1,0)`, Bottom-Right `(1,1,1,1)`, Full Fill `(0,0,1,1)`).
  - `FMargin` (Offsets): `Left` (Position X), `Top` (Position Y), `Right` (Size X), `Bottom` (Size Y). *Note: When anchors are stretched (Min != Max), `Right` and `Bottom` act as right/bottom margins rather than absolute width/height.*
  - `FVector2D` (Alignment): X (0.0 to 1.0), Y (0.0 to 1.0).
- **C++ Setter APIs**:
  - `CanvasSlot->SetAnchors(FAnchors InAnchors)`
  - `CanvasSlot->SetOffsets(FMargin InOffset)`
  - `CanvasSlot->SetPosition(FVector2D InPosition)` *(Modifies `Offsets.Left` and `Offsets.Top`)*
  - `CanvasSlot->SetSize(FVector2D InSize)` *(Modifies `Offsets.Right` and `Offsets.Bottom`)*
  - `CanvasSlot->SetAlignment(FVector2D InAlignment)`
  - `CanvasSlot->SetAutoSize(bool InbAutoSize)`
  - `CanvasSlot->SetZOrder(int32 InZOrder)`
  - `CanvasSlot->SetLayout(const FAnchorData& InLayoutData)`
- **C++ Getter APIs**:
  - `CanvasSlot->GetAnchors()`, `CanvasSlot->GetOffsets()`, `CanvasSlot->GetPosition()`, `CanvasSlot->GetSize()`, `CanvasSlot->GetAlignment()`, `CanvasSlot->GetAutoSize()`, `CanvasSlot->GetZOrder()`, `CanvasSlot->GetLayout()`.

### 3.2 `UVerticalBoxSlot`
- **Container**: `UVerticalBox`
- **Header**: `#include "Components/VerticalBoxSlot.h"`
- **Cast**: `UVerticalBoxSlot* VBSlot = Cast<UVerticalBoxSlot>(Widget->Slot);`
- **Key Structs**:
  - `FMargin`: `Padding` (`Left`, `Top`, `Right`, `Bottom`).
  - `FSlateChildSize`: `Value` (`float`), `SizeRule` (`ESlateSizeRule::Automatic` vs `ESlateSizeRule::Fill`).
  - `EHorizontalAlignment`: `HAlign_Left`, `HAlign_Center`, `HAlign_Right`, `HAlign_Fill`.
  - `EVerticalAlignment`: `VAlign_Top`, `VAlign_Center`, `VAlign_Bottom`, `VAlign_Fill`.
- **C++ Setter APIs**:
  - `VBSlot->SetPadding(FMargin InPadding)`
  - `VBSlot->SetSize(FSlateChildSize InSize)`
  - `VBSlot->SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)`
  - `VBSlot->SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)`
- **C++ Getter APIs**:
  - `VBSlot->GetPadding()`, `VBSlot->GetSize()`, `VBSlot->GetHorizontalAlignment()`, `VBSlot->GetVerticalAlignment()`.

### 3.3 `UHorizontalBoxSlot`
- **Container**: `UHorizontalBox`
- **Header**: `#include "Components/HorizontalBoxSlot.h"`
- **Cast**: `UHorizontalBoxSlot* HBSlot = Cast<UHorizontalBoxSlot>(Widget->Slot);`
- **C++ Setter APIs**:
  - `HBSlot->SetPadding(FMargin InPadding)`
  - `HBSlot->SetSize(FSlateChildSize InSize)`
  - `HBSlot->SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)`
  - `HBSlot->SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)`
- **C++ Getter APIs**:
  - `HBSlot->GetPadding()`, `HBSlot->GetSize()`, `HBSlot->GetHorizontalAlignment()`, `HBSlot->GetVerticalAlignment()`.

### 3.4 `UOverlaySlot`
- **Container**: `UOverlay`
- **Header**: `#include "Components/OverlaySlot.h"`
- **Cast**: `UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Widget->Slot);`
- **C++ Setter APIs**:
  - `OverlaySlot->SetPadding(FMargin InPadding)`
  - `OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)`
  - `OverlaySlot->SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)`
- **C++ Getter APIs**:
  - `OverlaySlot->GetPadding()`, `OverlaySlot->GetHorizontalAlignment()`, `OverlaySlot->GetVerticalAlignment()`.

### 3.5 `UGridSlot`
- **Container**: `UGridPanel`
- **Header**: `#include "Components/GridSlot.h"`
- **Cast**: `UGridSlot* GridSlot = Cast<UGridSlot>(Widget->Slot);`
- **C++ Setter APIs**:
  - `GridSlot->SetRow(int32 InRow)`
  - `GridSlot->SetColumn(int32 InColumn)`
  - `GridSlot->SetRowSpan(int32 InRowSpan)`
  - `GridSlot->SetColumnSpan(int32 InColumnSpan)`
  - `GridSlot->SetLayer(int32 InLayer)`
  - `GridSlot->SetNudge(FVector2D InNudge)`
  - `GridSlot->SetPadding(FMargin InPadding)`
  - `GridSlot->SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)`
  - `GridSlot->SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)`
- **C++ Getter APIs**:
  - `GridSlot->GetRow()`, `GridSlot->GetColumn()`, `GridSlot->GetRowSpan()`, `GridSlot->GetColumnSpan()`, `GridSlot->GetLayer()`, `GridSlot->GetNudge()`, `GridSlot->GetPadding()`, `GridSlot->GetHorizontalAlignment()`, `GridSlot->GetVerticalAlignment()`.

### 3.6 `UUniformGridSlot`
- **Container**: `UUniformGridPanel`
- **Header**: `#include "Components/UniformGridSlot.h"`
- **Cast**: `UUniformGridSlot* UniformSlot = Cast<UUniformGridSlot>(Widget->Slot);`
- **C++ Setter APIs**:
  - `UniformSlot->SetRow(int32 InRow)`
  - `UniformSlot->SetColumn(int32 InColumn)`
  - `UniformSlot->SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)`
  - `UniformSlot->SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)`
- **C++ Getter APIs**:
  - `UniformSlot->GetRow()`, `UniformSlot->GetColumn()`, `UniformSlot->GetHorizontalAlignment()`, `UniformSlot->GetVerticalAlignment()`.

### 3.7 `UScrollBoxSlot`
- **Container**: `UScrollBox`
- **Header**: `#include "Components/ScrollBoxSlot.h"`
- **Cast**: `UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(Widget->Slot);`
- **C++ Setter APIs**:
  - `ScrollSlot->SetPadding(FMargin InPadding)`
  - `ScrollSlot->SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)`
  - `ScrollSlot->SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)`

### 3.8 `UWrapBoxSlot`
- **Container**: `UWrapBox`
- **Header**: `#include "Components/WrapBoxSlot.h"`
- **Cast**: `UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(Widget->Slot);`
- **C++ Setter APIs**:
  - `WrapSlot->SetPadding(FMargin InPadding)`
  - `WrapSlot->SetFillEmptySpace(bool InbFillEmptySpace)`
  - `WrapSlot->SetFillSpanWhenLessThan(float InFillSpanWhenLessThan)`
  - `WrapSlot->SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)`
  - `WrapSlot->SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)`

### 3.9 `UWidgetSwitcherSlot`
- **Container**: `UWidgetSwitcher`
- **Header**: `#include "Components/WidgetSwitcherSlot.h"`
- **Cast**: `UWidgetSwitcherSlot* SwitcherSlot = Cast<UWidgetSwitcherSlot>(Widget->Slot);`
- **C++ Setter APIs**:
  - `SwitcherSlot->SetPadding(FMargin InPadding)`
  - `SwitcherSlot->SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)`
  - `SwitcherSlot->SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)`

### 3.10 `USizeBoxSlot`, `UScaleBoxSlot`, `UBorderSlot`
- **Containers**: `USizeBox`, `UScaleBox`, `UBorder` (Content Widgets)
- **C++ Setter APIs**: `SetPadding`, `SetHorizontalAlignment`, `SetVerticalAlignment`.

---

## 4. Multi-Format Parameter Parsing & Dual-Case Aliasing

To support both existing string formats (e.g. `"offsets": "10,20,100,50"`, `"anchors_min": "0,0"`) and new structured JSON formats specified in Spec 16 (e.g. `"anchors": { "min_x": 0, "min_y": 0, "max_x": 1, "max_y": 1 }`), parameter extraction in C++ must support dual-path parsing:

### 4.1 Anchors Parsing Logic
1. **Structured Object (`anchors`)**:
   - Check if `Params->HasField("anchors")` and `Params->GetObjectField("anchors")` exists.
   - Extract `min_x`, `min_y`, `max_x`, `max_y` (or `MinX`, `MinY`, `MaxX`, `MaxY`).
   - Construct `FAnchors(MinX, MinY, MaxX, MaxY)` and call `SetAnchors(...)`.
2. **String Format (`anchors_min` / `anchors_max`)**:
   - Parse `"0,0"` string via `ParseVector2D`.
   - Mutate `CanvasSlot->GetAnchors().Minimum` / `Maximum` and call `SetAnchors(...)`.

### 4.2 Offsets / Padding Parsing Logic
1. **Structured Object (`offsets` / `padding`)**:
   - Check if `Params->HasTypedField<EJson::Object>("offsets")` or `"padding"`.
   - Extract `left`, `top`, `right`, `bottom` (or `Left`, `Top`, `Right`, `Bottom`).
   - Construct `FMargin(Left, Top, Right, Bottom)` and set on slot.
2. **String Format (`"10,20,30,40"`)**:
   - Parse via `ParseMargin`.

### 4.3 Alignment Parsing Logic
1. **Structured Object (`alignment`)**: Extract `x`, `y` floats -> `FVector2D(X, Y)`.
2. **String Format (`alignment`)**: Parse `"0.5,0.5"` string via `ParseVector2D`.

### 4.4 Parameter Name Aliases
- `widget_blueprint_path` / `asset_path` -> resolve path via `ExpandWidgetAssetPath`.
- `widget_name` -> resolve widget in `WidgetBP->WidgetTree` via `FindWidgetByName`.

---

## 5. Editor State Modification, Dirty Marking, and Transactions

When modifying slot properties in an Unreal Editor C++ plugin action, strict editor object lifecycle steps must be followed:

1. **Transaction Logging (`FScopedTransaction`)**:
   - Enclose non-read-only tool calls inside `FScopedTransaction Transaction(FText::FromString(TEXT("AgentFramework Widget Action")));`.
   - If an error occurs during execution, call `Transaction.Cancel()`.

2. **Object Dirtying (`Modify()`)**:
   - Call `WidgetBP->Modify()` to mark the Widget Blueprint asset for modification.
   - Call `Widget->Slot->Modify()` to record state change on the slot object itself.

3. **Applying Property Setters**:
   - Call the dedicated slot setter methods (e.g. `CanvasSlot->SetAnchors(...)`, `VBSlot->SetPadding(...)`).

4. **Compilation & Package Dirtying (`CompileAndMarkDirty`)**:
   - Invoke `FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::SkipGarbageCollection)`.
   - Invoke `WidgetBP->GetOutermost()->MarkPackageDirty()`.
   - This ensures the Widget Blueprint bytecode is updated, UI layout changes are reflected in the Editor, and the asset file is flagged with a dirty dot (`*`) in the Content Browser for saving.

---

## 6. Audit Summary & Codebase Integration Roadmap

| Component | Current State | Required Enhancement for Spec 16 |
|---|---|---|
| Supported Tool Names | `set_widget_slot` in `GetSupportedToolNames()` | Add `set_widget_slot_properties` alias |
| Dispatch Router | Routes `set_widget_slot` to `ExecuteSetWidgetSlot` | Route both `set_widget_slot` and `set_widget_slot_properties` to `ExecuteSetWidgetSlot` |
| Asset Path Alias | Accepts `asset_path` | Accept both `asset_path` and `widget_blueprint_path` |
| Slot Parsing | String vector parsing (`"0,0"`, `"10,20,30,40"`) | Add nested JSON object extraction for `anchors`, `offsets`, `alignment`, `padding` |
| Slot Types Covered | `UCanvasPanelSlot`, `UVerticalBoxSlot`, `UHorizontalBoxSlot`, `UOverlaySlot`, `UGridSlot`, `UScrollBoxSlot`, `UWrapBoxSlot` | Add explicit support for `UUniformGridSlot`, `UWidgetSwitcherSlot`, `USizeBoxSlot`, `UScaleBoxSlot`, `UBorderSlot` |
| Editor Dirty/Undo | Uses `FScopedTransaction`, `WidgetBP->Modify()`, `Widget->Slot->Modify()`, `CompileAndMarkDirty()` | Existing pattern is fully compliant and robust |
