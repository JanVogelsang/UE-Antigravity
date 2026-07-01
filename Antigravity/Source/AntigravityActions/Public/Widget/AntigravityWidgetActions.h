// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

class UWidgetBlueprint;
class UWidget;
class UPanelSlot;

/**
 * FAntigravityWidgetActions
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
 * Supported tools (11):
 *   create_widget_blueprint, add_widget, set_widget_slot, set_widget_property,
 *   set_widget_font, set_widget_brush, bind_widget_event, remove_widget,
 *   get_widget_tree, compile_widget_blueprint, macro_create_basic_ui_menu
 */
class ANTIGRAVITYACTIONS_API FAntigravityWidgetActions : public IAntigravityActionExecutor
{
public:
	FAntigravityWidgetActions();
	virtual ~FAntigravityWidgetActions();

	// IAntigravityActionExecutor
	virtual FName GetActionName() const override;
	virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	// ======================================================================
	// Tool Handlers
	// ======================================================================

	/** Create a new Widget Blueprint asset with optional root widget and parent class. */
	FAntigravityActionResult ExecuteCreateWidgetBlueprint(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Add a widget to the tree hierarchy. */
	FAntigravityActionResult ExecuteAddWidget(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Configure the layout slot of a widget (anchors, offsets, padding, alignment, fill). */
	FAntigravityActionResult ExecuteSetWidgetSlot(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Set a property on a named widget via reflection. */
	FAntigravityActionResult ExecuteSetWidgetProperty(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Set font properties on a text widget (FSlateFontInfo). */
	FAntigravityActionResult ExecuteSetWidgetFont(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Set a brush (image/texture/solid color) on Image, Button, Border, ProgressBar widgets. */
	FAntigravityActionResult ExecuteSetWidgetBrush(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Bind a widget event (OnClicked, etc.) to a K2 event node in the EventGraph. */
	FAntigravityActionResult ExecuteBindWidgetEvent(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Remove a widget from the tree. */
	FAntigravityActionResult ExecuteRemoveWidget(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Read-only: return the full widget tree hierarchy as JSON. */
	FAntigravityActionResult ExecuteGetWidgetTree(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Compile a Widget Blueprint and return errors/warnings. */
	FAntigravityActionResult ExecuteCompileWidgetBlueprint(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Macro tool: programmatically generates a full UI menu widget in one call. */
	FAntigravityActionResult ExecuteMacroCreateBasicUIMenu(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Render a widget headlessly to a texture and return as Base64 image. */
	FAntigravityActionResult ExecuteCaptureWidget(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Instantiate a complete UI widget tree, configuring slot layout, properties, fonts, brushes, and event bindings in a single call. */
	FAntigravityActionResult ExecuteInstantiateUIHierarchy(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	// ======================================================================
	// Helpers
	// ======================================================================

	/** E.g. helper methods for batch setting of widget properties, slots, brushes, fonts and events. */
	static bool ApplyWidgetPropertyHelper(UWidgetBlueprint* WidgetBP, UWidget* TargetWidget, const FString& PropertyName, const FString& PropertyValue, FAntigravityActionResult& Result);
	static bool ApplyWidgetFontHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& FontParams, FAntigravityActionResult& Result);
	static bool ApplyWidgetBrushHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& BrushParams, FAntigravityActionResult& Result);
	static bool ApplyWidgetEventHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const FString& EventName, const FString& FunctionName, FAntigravityActionResult& Result);
	static bool ApplyWidgetSlotHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& SlotParams, FAntigravityActionResult& Result);

	/** Serialize the widget tree to JSON (with slot info, parent names). */
	static FString BuildWidgetTreeJson(UWidgetBlueprint* WidgetBlueprint);

	/** Resolve a widget class name (with or without U prefix) to a UClass. */
	static UClass* ResolveWidgetClass(const FString& ClassName);

	/** Load a Widget Blueprint from an asset path, adding error to Result if not found. */
	static UWidgetBlueprint* LoadWidgetBP(const FString& AssetPath, FAntigravityActionResult& Result);

	/** Find a widget by name in a Widget Blueprint, adding error to Result if not found. */
	static UWidget* FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FAntigravityActionResult& Result);

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

	/** Compile and mark dirty â€” common post-modification step. */
	static void CompileAndMarkDirty(UWidgetBlueprint* WidgetBP);
};
