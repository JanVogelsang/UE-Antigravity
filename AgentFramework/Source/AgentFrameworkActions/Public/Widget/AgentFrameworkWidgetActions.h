// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

class UWidgetBlueprint;
class UWidget;
class UPanelSlot;

/**
 * FAgentFrameworkWidgetActions
 *
 * Handles all UMG Widget Blueprint tool calls from the AI.
 *
 * Architecture:
 *   - Widget Blueprint creation: UWidgetBlueprintFactory + IAssetTools
 *   - Widget tree manipulation: UWidgetTree::ConstructWidget, FindWidget, UPanelWidget::AddChild
 *   - Slot configuration: UCanvasPanelSlot, UVerticalBoxSlot, etc. â€” anchors, offsets, padding, alignment
 *   - Property writes: reflection (FProperty) with allowlisted key guidance
 *   - Font configuration: FSlateFontInfo with engine font paths and typeface selection
 *   - Brush configuration: FSlateBrush with texture loading, solid colors, and 9-slice
 *   - Event binding: K2Node_ComponentBoundEvent creation for widget delegates
 *   - Widget removal: UPanelWidget::RemoveChild
 *   - Compilation: FKismetEditorUtilities::CompileBlueprint (same pipeline as Blueprint actors)
 *
 * UMG Widget Blueprint Dual Systems (analogous to Blueprint SCS + EventGraph):
 *   1. Widget Tree (SCS equivalent): UWidgetTree manages the visual hierarchy
 *      of UWidget objects (UPanelWidget containers + leaf widgets like TextBlock, Button).
 *      Mutate via add_widget / set_widget_property / set_widget_slot / set_widget_font / set_widget_brush.
 *   2. Widget Blueprint Graph: standard K2 nodes for event binding (OnClicked, etc.)
 *      â€” use bind_widget_event to create event nodes, then inject_blueprint_nodes_t3d for logic.
 *
 * Supported tools (17):
 *   create_widget_blueprint, add_widget, set_widget_slot, set_widget_slot_properties,
 *   set_widget_property, set_widget_font, set_widget_brush, bind_widget_event,
 *   remove_widget, get_widget_tree, compile_widget_blueprint, macro_create_basic_ui_menu,
 *   capture_widget, instantiate_ui_hierarchy, get_widget_info,
 *   clear_panel_children, get_widget_slots
 */
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkWidgetActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkWidgetActions();
	virtual ~FAgentFrameworkWidgetActions();

	// IAgentFrameworkActionExecutor
	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	// ======================================================================
	// Tool Handlers
	// ======================================================================

	/** Create a new Widget Blueprint asset with optional root widget and parent class. */
	FAgentFrameworkActionResult ExecuteCreateWidgetBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Add a widget to the tree hierarchy. */
	FAgentFrameworkActionResult ExecuteAddWidget(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Configure the layout slot of a widget (anchors, offsets, padding, alignment, fill). */
	FAgentFrameworkActionResult ExecuteSetWidgetSlot(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Configure layout slot properties on a child widget (anchors, offsets, padding, alignment, size, grid, z-order). Spec 16 tool: set_widget_slot_properties. */
	FAgentFrameworkActionResult ExecuteSetWidgetSlotProperties(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Set a property on a named widget via reflection. */
	FAgentFrameworkActionResult ExecuteSetWidgetProperty(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Set font properties on a text widget (FSlateFontInfo). */
	FAgentFrameworkActionResult ExecuteSetWidgetFont(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Set a brush (image/texture/solid color) on Image, Button, Border, ProgressBar widgets. */
	FAgentFrameworkActionResult ExecuteSetWidgetBrush(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Bind a widget event (OnClicked, etc.) to a K2 event node in the EventGraph. */
	FAgentFrameworkActionResult ExecuteBindWidgetEvent(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Remove a widget from the tree. */
	FAgentFrameworkActionResult ExecuteRemoveWidget(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Read-only: return the full widget tree hierarchy as JSON. */
	FAgentFrameworkActionResult ExecuteGetWidgetTree(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Compile a Widget Blueprint and return errors/warnings. */
	FAgentFrameworkActionResult ExecuteCompileWidgetBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Macro tool: programmatically generates a full UI menu widget in one call. */
	FAgentFrameworkActionResult ExecuteMacroCreateBasicUIMenu(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Render a widget headlessly to a texture and return as Base64 image. */
	FAgentFrameworkActionResult ExecuteCaptureWidget(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Instantiate a complete UI widget tree, configuring slot layout, properties, fonts, brushes, and event bindings in a single call. */
	FAgentFrameworkActionResult ExecuteInstantiateUIHierarchy(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Read-only: return detailed information for a specific widget in a Widget Blueprint. */
	FAgentFrameworkActionResult ExecuteGetWidgetInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Clear all children from a specified panel widget. */
	FAgentFrameworkActionResult ExecuteClearPanelChildren(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Read-only: list all widget slots and layout configurations in a Widget Blueprint. */
	FAgentFrameworkActionResult ExecuteGetWidgetSlots(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

public:
	// ======================================================================
	// Helpers
	// ======================================================================

	/** E.g. helper methods for batch setting of widget properties, slots, brushes, fonts and events. */
	static bool ApplyWidgetPropertyHelper(UWidgetBlueprint* WidgetBP, UWidget* TargetWidget, const FString& PropertyName, const FString& PropertyValue, FAgentFrameworkActionResult& Result);
	static bool ApplyWidgetFontHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& FontParams, FAgentFrameworkActionResult& Result);
	static bool ApplyWidgetBrushHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& BrushParams, FAgentFrameworkActionResult& Result);
	static bool ApplyWidgetEventHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const FString& EventName, const FString& FunctionName, FAgentFrameworkActionResult& Result);
	static bool ApplyWidgetSlotHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& SlotParams, FAgentFrameworkActionResult& Result);

	/** Serialize the widget tree to JSON (with slot info, parent names). */
	static FString BuildWidgetTreeJson(UWidgetBlueprint* WidgetBlueprint);

	/** Resolve a widget class name (with or without U prefix) to a UClass. */
	static UClass* ResolveWidgetClass(const FString& ClassName);

	/** Load a Widget Blueprint from an asset path, adding error to Result if not found. */
	static UWidgetBlueprint* LoadWidgetBP(const FString& AssetPath, FAgentFrameworkActionResult& Result);

	/** Find a widget by name in a Widget Blueprint, adding error to Result if not found. */
	static UWidget* FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FAgentFrameworkActionResult& Result);

	/** Parse a "X,Y" string to FVector2D. Returns false on parse failure. */
	static bool ParseVector2D(const FString& Str, FVector2D& OutVec);

	/** Parse a "Left,Top,Right,Bottom" or single-value string to FMargin. */
	static bool ParseMargin(const FString& Str, FMargin& OutMargin);

	/** Parse an alignment string (Left/Center/Right/Fill) to EHorizontalAlignment. */
	static EHorizontalAlignment ParseHAlign(const FString& Str);

	/** Parse an alignment string (Top/Center/Bottom/Fill) to EVerticalAlignment. */
	static EVerticalAlignment ParseVAlign(const FString& Str);

	/** Parse a linear color string "(R=1,G=0,B=0,A=1)" to FLinearColor. */
	static bool ParseLinearColor(const FString& Str, FLinearColor& OutColor);

	/** Compile and mark dirty — common post-modification step. */
	static void CompileAndMarkDirty(UWidgetBlueprint* WidgetBP);
};
