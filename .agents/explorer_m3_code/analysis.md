# Detailed Codebase Analysis: Widget Action `set_widget_slot_properties` (Spec 16)

## Executive Summary
This analysis evaluates the C++ codebase for Widget Actions (`FAgentFrameworkWidgetActions` in `AgentFrameworkWidgetActions.h` and `AgentFrameworkWidgetActions.cpp`) alongside Specification 16 from `PYTHON_FALLBACK_AUDIT.md`. The objective is to design the native C++ action implementation for `set_widget_slot_properties` to replace Python script fallbacks (`unreal.load_object` + slot mutation).

---

## 1. Specification 16 & Parameter Requirements

### Spec 16 Payload Definition (from `PYTHON_FALLBACK_AUDIT.md`)
Spec 16 specifies the tool route `set_widget_slot_properties` with the following JSON schema:
- **`widget_blueprint_path`** (`string`, required): Asset package path to the Target Widget Blueprint (e.g., `/Game/UI/WBP_MainMenu`).
- **`widget_name`** (`string`, required): Name of the child widget inside the `WidgetTree` (e.g., `Btn_Start`).
- **`anchors`** (`object`, optional): Anchor bounds `{ "min_x": float, "min_y": float, "max_x": float, "max_y": float }`.
- **`alignment`** (`object`, optional): Normalized pivot alignment `{ "x": float, "y": float }`.
- **`offsets`** (`object`, optional): Margin offsets `{ "left": float, "top": float, "right": float, "bottom": float }`.
- **`slot_properties`** (`object`, optional): Nested container object wrapping layout properties.

### Parameter Name Dual-Alias Resolution Matrix
To support both legacy agent calls, Python script conversions, and Spec 16 formats, parameter extraction must resolve the following aliases:

| Canonical Field | Primary Alias (snake_case) | PascalCase Alias | Alternative / Legacy Aliases | Expected Types |
|---|---|---|---|---|
| Asset Path | `widget_blueprint_path` | `WidgetBlueprintPath` | `asset_path`, `AssetPath`, `TargetAsset` | String |
| Widget Name | `widget_name` | `WidgetName` | `name`, `Name` | String |
| Anchors | `anchors` | `Anchors` | `anchors_min`, `anchors_max` | Object or String (`"MinX,MinY,MaxX,MaxY"`) |
| Alignment | `alignment` | `Alignment` | `align`, `Align` | Object (`{"x":0.5,"y":0.5}`) or String (`"X,Y"`) |
| Offsets | `offsets` | `Offsets` | `margin`, `Margin` | Object (`{"left":0,"top":0,"right":100,"bottom":50}`) or String (`"L,T,R,B"`) |
| Padding | `padding` | `Padding` | `pad`, `Pad` | Object (`{"left":10,"top":5,"right":10,"bottom":5}`) or String (`"L,T,R,B"`) |
| Horizontal Alignment | `h_align` | `HAlign` | `horizontal_alignment`, `HorizontalAlignment` | String (`"Left"`, `"Center"`, `"Right"`, `"Fill"`) |
| Vertical Alignment | `v_align` | `VAlign` | `vertical_alignment`, `VerticalAlignment` | String (`"Top"`, `"Center"`, `"Bottom"`, `"Fill"`) |
| Size Rule | `size_rule` | `SizeRule` | `size`, `Size` | String (`"Fill"`, `"Auto"`) |
| Auto Size | `auto_size` | `AutoSize` | `bAutoSize` | Boolean |
| Z Order | `z_order` | `ZOrder` | `zorder` | Integer |
| Row / Column | `row`, `column` | `Row`, `Column` | `grid_row`, `grid_column` | Integer |
| Row/Col Span | `row_span`, `column_span` | `RowSpan`, `ColumnSpan` | `rowspan`, `columnspan` | Integer |
| Fill Empty Space | `fill_empty_space` | `FillEmptySpace` | `fill` | Boolean |
| Container Object | `slot_properties` | `SlotProperties` | `slot_params`, `SlotParams` | Object |

---

## 2. Supported UMG Slot Types & Property Mutations

The C++ implementation in `AgentFrameworkWidgetActions.cpp` currently supports slot mutations across major UMG panel slot types. Below is the complete catalog of supported slot classes and their corresponding property setters:

### 1. `UCanvasPanelSlot` (Parent: `UCanvasPanel`)
- **Anchors**: `SetAnchors(FAnchors)` parsed from `anchors` JSON object (`min_x`, `min_y`, `max_x`, `max_y`) or `anchors_min`/`anchors_max` strings.
- **Offsets**: `SetOffsets(FMargin)` parsed from `offsets` object (`left`, `top`, `right`, `bottom`) or `"L,T,R,B"` string. Note: `Left`/`Top` represent position offset; `Right`/`Bottom` represent size or margin depending on anchors.
- **Alignment**: `SetAlignment(FVector2D)` parsed from `alignment` object (`x`, `y`) or `"X,Y"` string.
- **AutoSize**: `SetAutoSize(bool)` from `auto_size` boolean.
- **ZOrder**: `SetZOrder(int32)` from `z_order` integer.

### 2. `UVerticalBoxSlot` (Parent: `UVerticalBox`)
- **Padding**: `SetPadding(FMargin)` from `padding` object or string.
- **Size**: `SetSize(FSlateChildSize)` where `SizeRule` is set to `ESlateSizeRule::Fill` or `ESlateSizeRule::Automatic` based on `size_rule`.
- **HorizontalAlignment**: `SetHorizontalAlignment(EHorizontalAlignment)` parsed via `ParseHAlign`.
- **VerticalAlignment**: `SetVerticalAlignment(EVerticalAlignment)` parsed via `ParseVAlign`.

### 3. `UHorizontalBoxSlot` (Parent: `UHorizontalBox`)
- **Padding**: `SetPadding(FMargin)` from `padding` object or string.
- **Size**: `SetSize(FSlateChildSize)` (`Fill` / `Auto`).
- **HorizontalAlignment**: `SetHorizontalAlignment(EHorizontalAlignment)`.
- **VerticalAlignment**: `SetVerticalAlignment(EVerticalAlignment)`.

### 4. `UOverlaySlot` (Parent: `UOverlay`)
- **Padding**: `SetPadding(FMargin)`.
- **HorizontalAlignment**: `SetHorizontalAlignment(EHorizontalAlignment)`.
- **VerticalAlignment**: `SetVerticalAlignment(EVerticalAlignment)`.

### 5. `UGridSlot` (Parent: `UGridPanel`)
- **Row**: `SetRow(int32)`.
- **Column**: `SetColumn(int32)`.
- **RowSpan**: `SetRowSpan(int32)`.
- **ColumnSpan**: `SetColumnSpan(int32)`.
- **Padding**: `SetPadding(FMargin)`.
- **HorizontalAlignment**: `SetHorizontalAlignment(EHorizontalAlignment)`.
- **VerticalAlignment**: `SetVerticalAlignment(EVerticalAlignment)`.

### 6. `UScrollBoxSlot` (Parent: `UScrollBox`)
- **Padding**: `SetPadding(FMargin)`.
- **HorizontalAlignment**: `SetHorizontalAlignment(EHorizontalAlignment)`.
- **VerticalAlignment**: `SetVerticalAlignment(EVerticalAlignment)`.

### 7. `UWrapBoxSlot` (Parent: `UWrapBox`)
- **Padding**: `SetPadding(FMargin)`.
- **HorizontalAlignment**: `SetHorizontalAlignment(EHorizontalAlignment)`.
- **VerticalAlignment**: `SetVerticalAlignment(EVerticalAlignment)`.
- **FillEmptySpace**: `SetFillEmptySpace(bool)`.

### 8. Additional Slot Types to Include
- `UUniformGridSlot` (`SetRow`, `SetColumn`, `SetHorizontalAlignment`, `SetVerticalAlignment`)
- `UWidgetSwitcherSlot` (`SetPadding`, `SetHorizontalAlignment`, `SetVerticalAlignment`)
- `UScaleBoxSlot` (`SetPadding`, `SetHorizontalAlignment`, `SetVerticalAlignment`)
- `UBorderSlot` (`SetPadding`, `SetHorizontalAlignment`, `SetVerticalAlignment`)
- `USizeBoxSlot` (`SetPadding`, `SetHorizontalAlignment`, `SetVerticalAlignment`)

---

## 3. UWidget & UWidgetBlueprint Resolution Workflow

To retrieve a target `UWidget` from `widget_blueprint_path` and `widget_name`:

```cpp
// 1. Resolve & Load Widget Blueprint Asset
FString AssetPath;
if (!ResolveAssetPathAlias(Params, AssetPath, Result.Errors)) return Result;

UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
if (!IsValid(WidgetBP)) return Result;

// 2. Resolve & Find Target UWidget by Name inside WidgetTree
FString WidgetName;
if (!ResolveWidgetNameAlias(Params, WidgetName, Result.Errors)) return Result;

UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
if (!IsValid(Widget)) return Result;

// 3. Verify Slot Assignment
if (!IsValid(Widget->Slot))
{
    Result.Errors.Add(FString::Printf(
        TEXT("Widget '%s' has no slot. Root widgets do not have slots, or widget is not attached to a parent panel."),
        *WidgetName
    ));
    return Result;
}
```

### Key Helpers in `FAgentFrameworkWidgetActions`:
- `LoadWidgetBP(const FString& AssetPath, FAgentFrameworkActionResult& Result)`: Expands path (e.g. `/UI/WBP_MainMenu` -> `/Game/UI/WBP_MainMenu`), loads `UWidgetBlueprint` via `StaticLoadObject`, and verifies `WidgetTree`.
- `FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FAgentFrameworkActionResult& Result)`: Queries `WidgetBP->WidgetTree->FindWidget(FName(*WidgetName))`. If null, performs case-insensitive fallback search across `GetAllWidgets()`.

---

## 4. Error Handling and Response Structure

### Result Payload (`FAgentFrameworkActionResult`)
- **`bSuccess`** (`bool`): `true` if at least one slot property was successfully modified.
- **`ResultMessage`** (`FString`): Summary of applied slot settings (e.g., `"Configured slot on 'Btn_Start': anchors=(0.0,0.0)-(1.0,1.0) offsets=(0,0,200,50)"`).
- **`ModifiedAssets`** (`TArray<FString>`): Contains target `AssetPath` for dirty-marking and package saving.
- **`Errors`** (`TArray<FString>`): Descriptive error messages on validation or runtime failure.

### Validation & Error Scenarios
1. **Missing Asset Path**: Returns error if none of `widget_blueprint_path`, `asset_path`, `TargetAsset`, `AssetPath` are provided.
2. **Invalid Asset Path Format**: Returns error if path does not start with `/Game/`.
3. **Asset Not Found / Not Widget Blueprint**: Returns error if `StaticLoadObject` fails.
4. **Widget Not Found**: Returns error listing available widget names in `WidgetTree`.
5. **No Slot Attached**: Returns error explaining root widget or missing parent attachment.
6. **No Valid Slot Properties Provided**: Returns error listing slot type name and valid property keys.

---

## 5. Implementation Recommendation

1. **Tool Name Registration**:
   - Register `set_widget_slot_properties` in `FAgentFrameworkWidgetActions::GetSupportedToolNames()` alongside `set_widget_slot`.
2. **Dispatching**:
   - In `ExecuteAction()`, route both `set_widget_slot` and `set_widget_slot_properties` to `ExecuteSetWidgetSlot()`.
3. **Unified Parameter Extractor**:
   - Enhance `ExecuteSetWidgetSlot()` and `ApplyWidgetSlotHelper()` to handle JSON objects (`anchors`, `offsets`, `alignment`, `padding`), nested `slot_properties` containers, and dual PascalCase/snake_case aliases.
4. **Post-Mutation**:
   - Call `CompileAndMarkDirty(WidgetBP)` to ensure design-time UMG preview and compiled bytecode reflect slot mutations immediately.
