// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Widget/AgentFrameworkWidgetActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"

// UMG Runtime — Panels
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/ContentWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/SizeBoxSlot.h"
#include "Components/BackgroundBlur.h"
#include "Components/InvalidationBox.h"
#include "Components/RetainerBox.h"
#include "Components/NamedSlot.h"

// UMG Runtime — Leaf Widgets
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/Spacer.h"
#include "Components/Throbber.h"
#include "Components/CircularThrobber.h"
#include "Components/ExpandableArea.h"
#include "Components/MenuAnchor.h"

// UMG Editor (Widget Blueprint factory and asset type)
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Viewport/AgentFrameworkViewportActions.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"

// Blueprint Graph (for event binding)
#include "K2Node_ComponentBoundEvent.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"

// Asset management
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

// Serialization / JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Misc
#include "ScopedTransaction.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "Blueprint/UserWidget.h"

// ============================================================================
// Statics / Lifecycle
// ============================================================================

namespace
{
	FString ExpandWidgetAssetPath(const FString& InPath)
	{
		if (InPath.IsEmpty()) return InPath;
		if (InPath.StartsWith(TEXT("/Game/")) || InPath.StartsWith(TEXT("/Engine/")) || InPath.StartsWith(TEXT("/Script/")) || InPath.StartsWith(TEXT("/Temp/")))
		{
			return InPath;
		}
		if (InPath.StartsWith(TEXT("/")))
		{
			return TEXT("/Game") + InPath;
		}
		return TEXT("/Game/") + InPath;
	}

	FString CompressWidgetAssetPath(const FString& InPath)
	{
		if (InPath.StartsWith(TEXT("/Game/")))
		{
			return InPath.Mid(6);
		}
		return InPath;
	}

	bool ResolveWidgetAssetPath(const TSharedPtr<FJsonObject>& Params, FString& OutAssetPath, TArray<FString>& OutErrors)
	{
		FString RawPath;
		if (Params->TryGetStringField(TEXT("asset_path"), RawPath) ||
			Params->TryGetStringField(TEXT("widget_blueprint_path"), RawPath) ||
			Params->TryGetStringField(TEXT("AssetPath"), RawPath) ||
			Params->TryGetStringField(TEXT("WidgetBlueprintPath"), RawPath) ||
			Params->TryGetStringField(TEXT("TargetAsset"), RawPath))
		{
			OutAssetPath = ExpandWidgetAssetPath(RawPath);
			return true;
		}
		OutErrors.Add(TEXT("Missing required asset path parameter (asset_path or widget_blueprint_path)."));
		return false;
	}

	bool ResolveWidgetName(const TSharedPtr<FJsonObject>& Params, FString& OutWidgetName, TArray<FString>& OutErrors)
	{
		if (Params->TryGetStringField(TEXT("widget_name"), OutWidgetName) ||
			Params->TryGetStringField(TEXT("WidgetName"), OutWidgetName) ||
			Params->TryGetStringField(TEXT("name"), OutWidgetName) ||
			Params->TryGetStringField(TEXT("Name"), OutWidgetName))
		{
			if (!OutWidgetName.IsEmpty())
			{
				return true;
			}
		}
		OutErrors.Add(TEXT("Missing required widget name parameter (widget_name or WidgetName)."));
		return false;
	}

	TSharedPtr<FJsonObject> GetEffectiveSlotParams(const TSharedPtr<FJsonObject>& InParams)
	{
		if (!InParams.IsValid()) return MakeShared<FJsonObject>();

		const TSharedPtr<FJsonObject>* ContainerObj = nullptr;
		if (InParams->TryGetObjectField(TEXT("slot_properties"), ContainerObj) ||
			InParams->TryGetObjectField(TEXT("SlotProperties"), ContainerObj) ||
			InParams->TryGetObjectField(TEXT("slot_params"), ContainerObj) ||
			InParams->TryGetObjectField(TEXT("SlotParams"), ContainerObj))
		{
			if (ContainerObj && ContainerObj->IsValid())
			{
				TSharedPtr<FJsonObject> Merged = MakeShared<FJsonObject>();
				for (const auto& Pair : (*ContainerObj)->Values)
				{
					Merged->SetField(Pair.Key, Pair.Value);
				}
				for (const auto& Pair : InParams->Values)
				{
					if (!Merged->HasField(Pair.Key))
					{
						Merged->SetField(Pair.Key, Pair.Value);
					}
				}
				return Merged;
			}
		}
		return InParams;
	}

	bool TryGetAnchors(const TSharedPtr<FJsonObject>& InParams, FAnchors& OutAnchors)
	{
		if (!InParams.IsValid()) return false;

		const TSharedPtr<FJsonObject>* AnchorsObj = nullptr;
		if (InParams->TryGetObjectField(TEXT("anchors"), AnchorsObj) ||
			InParams->TryGetObjectField(TEXT("Anchors"), AnchorsObj))
		{
			if (AnchorsObj && AnchorsObj->IsValid())
			{
				double MinX = 0.0, MinY = 0.0, MaxX = 0.0, MaxY = 0.0;
				bool bHasMinX = (*AnchorsObj)->TryGetNumberField(TEXT("min_x"), MinX) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("MinX"), MinX) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("minimum_x"), MinX) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("x"), MinX) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("X"), MinX);

				bool bHasMinY = (*AnchorsObj)->TryGetNumberField(TEXT("min_y"), MinY) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("MinY"), MinY) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("minimum_y"), MinY) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("y"), MinY) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("Y"), MinY);

				bool bHasMaxX = (*AnchorsObj)->TryGetNumberField(TEXT("max_x"), MaxX) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("MaxX"), MaxX) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("maximum_x"), MaxX);

				bool bHasMaxY = (*AnchorsObj)->TryGetNumberField(TEXT("max_y"), MaxY) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("MaxY"), MaxY) ||
				                (*AnchorsObj)->TryGetNumberField(TEXT("maximum_y"), MaxY);

				if (!bHasMaxX) MaxX = MinX;
				if (!bHasMaxY) MaxY = MinY;

				if (bHasMinX || bHasMinY || bHasMaxX || bHasMaxY)
				{
					OutAnchors.Minimum = FVector2D(MinX, MinY);
					OutAnchors.Maximum = FVector2D(MaxX, MaxY);
					return true;
				}
			}
		}

		FString AnchorsStr, MinStr, MaxStr;
		if (InParams->TryGetStringField(TEXT("anchors"), AnchorsStr) ||
			InParams->TryGetStringField(TEXT("Anchors"), AnchorsStr))
		{
			TArray<FString> Parts;
			AnchorsStr.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() == 4)
			{
				OutAnchors.Minimum = FVector2D(FCString::Atof(*Parts[0].TrimStartAndEnd()), FCString::Atof(*Parts[1].TrimStartAndEnd()));
				OutAnchors.Maximum = FVector2D(FCString::Atof(*Parts[2].TrimStartAndEnd()), FCString::Atof(*Parts[3].TrimStartAndEnd()));
				return true;
			}
			else if (Parts.Num() == 2)
			{
				FVector2D Vec(FCString::Atof(*Parts[0].TrimStartAndEnd()), FCString::Atof(*Parts[1].TrimStartAndEnd()));
				OutAnchors.Minimum = Vec;
				OutAnchors.Maximum = Vec;
				return true;
			}
		}

		bool bFoundMin = false, bFoundMax = false;
		if (InParams->TryGetStringField(TEXT("anchors_min"), MinStr) ||
			InParams->TryGetStringField(TEXT("AnchorsMin"), MinStr) ||
			InParams->TryGetStringField(TEXT("anchors_minimum"), MinStr))
		{
			FVector2D MinVec;
			if (FAgentFrameworkWidgetActions::ParseVector2D(MinStr, MinVec))
			{
				OutAnchors.Minimum = MinVec;
				bFoundMin = true;
			}
		}

		if (InParams->TryGetStringField(TEXT("anchors_max"), MaxStr) ||
			InParams->TryGetStringField(TEXT("AnchorsMax"), MaxStr) ||
			InParams->TryGetStringField(TEXT("anchors_maximum"), MaxStr))
		{
			FVector2D MaxVec;
			if (FAgentFrameworkWidgetActions::ParseVector2D(MaxStr, MaxVec))
			{
				OutAnchors.Maximum = MaxVec;
				bFoundMax = true;
			}
		}

		if (bFoundMin && !bFoundMax)
		{
			OutAnchors.Maximum = OutAnchors.Minimum;
			return true;
		}
		return (bFoundMin || bFoundMax);
	}

	bool TryGetMargin(const TSharedPtr<FJsonObject>& InParams, const FString& FieldNameSnake, const FString& FieldNamePascal, FMargin& OutMargin)
	{
		if (!InParams.IsValid()) return false;

		const TSharedPtr<FJsonObject>* MarginObj = nullptr;
		if (InParams->TryGetObjectField(FieldNameSnake, MarginObj) ||
			InParams->TryGetObjectField(FieldNamePascal, MarginObj) ||
			(FieldNameSnake == TEXT("offsets") && InParams->TryGetObjectField(TEXT("margin"), MarginObj)) ||
			(FieldNameSnake == TEXT("offsets") && InParams->TryGetObjectField(TEXT("Margin"), MarginObj)) ||
			(FieldNameSnake == TEXT("padding") && InParams->TryGetObjectField(TEXT("pad"), MarginObj)) ||
			(FieldNameSnake == TEXT("padding") && InParams->TryGetObjectField(TEXT("Pad"), MarginObj)))
		{
			if (MarginObj && MarginObj->IsValid())
			{
				double Left = 0.0, Top = 0.0, Right = 0.0, Bottom = 0.0;
				bool bHasLeft = (*MarginObj)->TryGetNumberField(TEXT("left"), Left) ||
				                (*MarginObj)->TryGetNumberField(TEXT("Left"), Left) ||
				                (*MarginObj)->TryGetNumberField(TEXT("l"), Left) ||
				                (*MarginObj)->TryGetNumberField(TEXT("x"), Left) ||
				                (*MarginObj)->TryGetNumberField(TEXT("X"), Left);

				bool bHasTop = (*MarginObj)->TryGetNumberField(TEXT("top"), Top) ||
				               (*MarginObj)->TryGetNumberField(TEXT("Top"), Top) ||
				               (*MarginObj)->TryGetNumberField(TEXT("t"), Top) ||
				               (*MarginObj)->TryGetNumberField(TEXT("y"), Top) ||
				               (*MarginObj)->TryGetNumberField(TEXT("Y"), Top);

				bool bHasRight = (*MarginObj)->TryGetNumberField(TEXT("right"), Right) ||
				                 (*MarginObj)->TryGetNumberField(TEXT("Right"), Right) ||
				                 (*MarginObj)->TryGetNumberField(TEXT("r"), Right) ||
				                 (*MarginObj)->TryGetNumberField(TEXT("width"), Right) ||
				                 (*MarginObj)->TryGetNumberField(TEXT("Width"), Right) ||
				                 (*MarginObj)->TryGetNumberField(TEXT("w"), Right);

				bool bHasBottom = (*MarginObj)->TryGetNumberField(TEXT("bottom"), Bottom) ||
				                  (*MarginObj)->TryGetNumberField(TEXT("Bottom"), Bottom) ||
				                  (*MarginObj)->TryGetNumberField(TEXT("b"), Bottom) ||
				                  (*MarginObj)->TryGetNumberField(TEXT("height"), Bottom) ||
				                  (*MarginObj)->TryGetNumberField(TEXT("Height"), Bottom) ||
				                  (*MarginObj)->TryGetNumberField(TEXT("h"), Bottom);

				double Uniform = 0.0;
				if ((*MarginObj)->TryGetNumberField(TEXT("uniform"), Uniform) ||
					(*MarginObj)->TryGetNumberField(TEXT("Uniform"), Uniform))
				{
					OutMargin = FMargin(Uniform);
					return true;
				}

				if (bHasLeft || bHasTop || bHasRight || bHasBottom)
				{
					OutMargin = FMargin(Left, Top, Right, Bottom);
					return true;
				}
			}
		}

		double NumVal = 0.0;
		if (InParams->TryGetNumberField(FieldNameSnake, NumVal) ||
			InParams->TryGetNumberField(FieldNamePascal, NumVal))
		{
			OutMargin = FMargin(NumVal);
			return true;
		}

		FString MarginStr;
		if (InParams->TryGetStringField(FieldNameSnake, MarginStr) ||
			InParams->TryGetStringField(FieldNamePascal, MarginStr) ||
			(FieldNameSnake == TEXT("offsets") && InParams->TryGetStringField(TEXT("margin"), MarginStr)) ||
			(FieldNameSnake == TEXT("offsets") && InParams->TryGetStringField(TEXT("Margin"), MarginStr)) ||
			(FieldNameSnake == TEXT("padding") && InParams->TryGetStringField(TEXT("pad"), MarginStr)) ||
			(FieldNameSnake == TEXT("padding") && InParams->TryGetStringField(TEXT("Pad"), MarginStr)))
		{
			if (FAgentFrameworkWidgetActions::ParseMargin(MarginStr, OutMargin))
			{
				return true;
			}
		}

		return false;
	}

	bool TryGetAlignment(const TSharedPtr<FJsonObject>& InParams, FVector2D& OutAlignment)
	{
		if (!InParams.IsValid()) return false;

		const TSharedPtr<FJsonObject>* AlignObj = nullptr;
		if (InParams->TryGetObjectField(TEXT("alignment"), AlignObj) ||
			InParams->TryGetObjectField(TEXT("Alignment"), AlignObj) ||
			InParams->TryGetObjectField(TEXT("align"), AlignObj) ||
			InParams->TryGetObjectField(TEXT("Align"), AlignObj))
		{
			if (AlignObj && AlignObj->IsValid())
			{
				double X = 0.0, Y = 0.0;
				bool bHasX = (*AlignObj)->TryGetNumberField(TEXT("x"), X) ||
				             (*AlignObj)->TryGetNumberField(TEXT("X"), X) ||
				             (*AlignObj)->TryGetNumberField(TEXT("horizontal"), X) ||
				             (*AlignObj)->TryGetNumberField(TEXT("h"), X);

				bool bHasY = (*AlignObj)->TryGetNumberField(TEXT("y"), Y) ||
				             (*AlignObj)->TryGetNumberField(TEXT("Y"), Y) ||
				             (*AlignObj)->TryGetNumberField(TEXT("vertical"), Y) ||
				             (*AlignObj)->TryGetNumberField(TEXT("v"), Y);

				if (bHasX || bHasY)
				{
					OutAlignment = FVector2D(X, Y);
					return true;
				}
			}
		}

		double Scalar = 0.0;
		if (InParams->TryGetNumberField(TEXT("alignment"), Scalar) ||
			InParams->TryGetNumberField(TEXT("Alignment"), Scalar))
		{
			OutAlignment = FVector2D(Scalar, Scalar);
			return true;
		}

		FString AlignStr;
		if (InParams->TryGetStringField(TEXT("alignment"), AlignStr) ||
			InParams->TryGetStringField(TEXT("Alignment"), AlignStr) ||
			InParams->TryGetStringField(TEXT("align"), AlignStr) ||
			InParams->TryGetStringField(TEXT("Align"), AlignStr))
		{
			if (FAgentFrameworkWidgetActions::ParseVector2D(AlignStr, OutAlignment))
			{
				return true;
			}
		}

		return false;
	}

	bool TryGetSizeRule(const TSharedPtr<FJsonObject>& InParams, FString& OutSizeRuleStr, ESlateSizeRule::Type& OutSizeRule)
	{
		if (!InParams.IsValid()) return false;
		if (InParams->TryGetStringField(TEXT("size_rule"), OutSizeRuleStr) ||
			InParams->TryGetStringField(TEXT("SizeRule"), OutSizeRuleStr) ||
			InParams->TryGetStringField(TEXT("size"), OutSizeRuleStr) ||
			InParams->TryGetStringField(TEXT("Size"), OutSizeRuleStr))
		{
			if (OutSizeRuleStr.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
			{
				OutSizeRule = ESlateSizeRule::Fill;
			}
			else
			{
				OutSizeRule = ESlateSizeRule::Automatic;
			}
			return true;
		}
		return false;
	}

	bool TryGetHAlign(const TSharedPtr<FJsonObject>& InParams, FString& OutStr, EHorizontalAlignment& OutHAlign)
	{
		if (!InParams.IsValid()) return false;
		if (InParams->TryGetStringField(TEXT("h_align"), OutStr) ||
			InParams->TryGetStringField(TEXT("HAlign"), OutStr) ||
			InParams->TryGetStringField(TEXT("horizontal_alignment"), OutStr) ||
			InParams->TryGetStringField(TEXT("HorizontalAlignment"), OutStr))
		{
			OutHAlign = FAgentFrameworkWidgetActions::ParseHAlign(OutStr);
			return true;
		}
		return false;
	}

	bool TryGetVAlign(const TSharedPtr<FJsonObject>& InParams, FString& OutStr, EVerticalAlignment& OutVAlign)
	{
		if (!InParams.IsValid()) return false;
		if (InParams->TryGetStringField(TEXT("v_align"), OutStr) ||
			InParams->TryGetStringField(TEXT("VAlign"), OutStr) ||
			InParams->TryGetStringField(TEXT("vertical_alignment"), OutStr) ||
			InParams->TryGetStringField(TEXT("VerticalAlignment"), OutStr))
		{
			OutVAlign = FAgentFrameworkWidgetActions::ParseVAlign(OutStr);
			return true;
		}
		return false;
	}

	bool TryGetBoolValue(const TSharedPtr<FJsonObject>& InParams, const FString& K1, const FString& K2, const FString& K3, bool& OutVal)
	{
		if (!InParams.IsValid()) return false;
		if (InParams->HasField(K1) && InParams->TryGetBoolField(K1, OutVal)) return true;
		if (!K2.IsEmpty() && InParams->HasField(K2) && InParams->TryGetBoolField(K2, OutVal)) return true;
		if (!K3.IsEmpty() && InParams->HasField(K3) && InParams->TryGetBoolField(K3, OutVal)) return true;

		FString StrVal;
		if (InParams->TryGetStringField(K1, StrVal) || (!K2.IsEmpty() && InParams->TryGetStringField(K2, StrVal)) || (!K3.IsEmpty() && InParams->TryGetStringField(K3, StrVal)))
		{
			OutVal = StrVal.ToBool();
			return true;
		}
		return false;
	}

	bool TryGetIntValue(const TSharedPtr<FJsonObject>& InParams, const FString& K1, const FString& K2, const FString& K3, int32& OutVal)
	{
		if (!InParams.IsValid()) return false;
		double Num = 0.0;
		if (InParams->TryGetNumberField(K1, Num) || (!K2.IsEmpty() && InParams->TryGetNumberField(K2, Num)) || (!K3.IsEmpty() && InParams->TryGetNumberField(K3, Num)))
		{
			OutVal = static_cast<int32>(Num);
			return true;
		}
		FString StrVal;
		if (InParams->TryGetStringField(K1, StrVal) || (!K2.IsEmpty() && InParams->TryGetStringField(K2, StrVal)) || (!K3.IsEmpty() && InParams->TryGetStringField(K3, StrVal)))
		{
			OutVal = FCString::Atoi(*StrVal);
			return true;
		}
		return false;
	}

	bool TryGetFloatValue(const TSharedPtr<FJsonObject>& InParams, const FString& K1, const FString& K2, float& OutVal)
	{
		if (!InParams.IsValid()) return false;
		double Num = 0.0;
		if (InParams->TryGetNumberField(K1, Num) || (!K2.IsEmpty() && InParams->TryGetNumberField(K2, Num)))
		{
			OutVal = static_cast<float>(Num);
			return true;
		}
		FString StrVal;
		if (InParams->TryGetStringField(K1, StrVal) || (!K2.IsEmpty() && InParams->TryGetStringField(K2, StrVal)))
		{
			OutVal = FCString::Atof(*StrVal);
			return true;
		}
		return false;
	}
}

FAgentFrameworkWidgetActions::FAgentFrameworkWidgetActions() {}
FAgentFrameworkWidgetActions::~FAgentFrameworkWidgetActions() {}

FName FAgentFrameworkWidgetActions::GetActionName() const { return FName(TEXT("Widget")); }

TArray<FString> FAgentFrameworkWidgetActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_widget_blueprint"),
		TEXT("add_widget"),
		TEXT("set_widget_slot"),
		TEXT("set_widget_slot_properties"),
		TEXT("set_widget_property"),
		TEXT("set_widget_font"),
		TEXT("set_widget_brush"),
		TEXT("bind_widget_event"),
		TEXT("remove_widget"),
		TEXT("get_widget_tree"),
		TEXT("compile_widget_blueprint"),
		TEXT("macro_create_basic_ui_menu"),
		TEXT("capture_widget"),
		TEXT("instantiate_ui_hierarchy"),
		TEXT("get_widget_info"),
		TEXT("clear_panel_children"),
		TEXT("get_widget_slots")
	};
}

bool FAgentFrameworkWidgetActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString RawAssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), RawAssetPath))
	{
		if (!Params->TryGetStringField(TEXT("widget_blueprint_path"), RawAssetPath))
		{
			if (!Params->TryGetStringField(TEXT("AssetPath"), RawAssetPath))
			{
				Params->TryGetStringField(TEXT("WidgetBlueprintPath"), RawAssetPath);
			}
		}
	}
	if (!RawAssetPath.IsEmpty())
	{
		Params->SetStringField(TEXT("asset_path"), ExpandWidgetAssetPath(RawAssetPath));
	}

	FString RawWidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), RawWidgetName))
	{
		if (Params->TryGetStringField(TEXT("WidgetName"), RawWidgetName))
		{
			Params->SetStringField(TEXT("widget_name"), RawWidgetName);
		}
	}

	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true))
	{
		return false;
	}

	if (!AssetPath.StartsWith(TEXT("/Game/")))
	{
		OutErrors.Add(FString::Printf(TEXT("asset_path '%s' must start with /Game/. Example: /Game/UI/WBP_MainMenu"), *AssetPath));
		return false;
	}

	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, OutErrors, false);

	if (ToolName == TEXT("add_widget"))
	{
		FString WidgetClass, WidgetName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_class"), WidgetClass, OutErrors, true) ||
			!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("set_widget_slot") || ToolName == TEXT("set_widget_slot_properties"))
	{
		FString WidgetName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("set_widget_property"))
	{
		FString WidgetName, PropertyName, PropertyValue;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, OutErrors, true) ||
			!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("property_name"), PropertyName, OutErrors, true) ||
			!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("property_value"), PropertyValue, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("set_widget_font"))
	{
		FString WidgetName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("set_widget_brush"))
	{
		FString WidgetName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("bind_widget_event"))
	{
		FString WidgetName, EventName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, OutErrors, true) ||
			!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("event_name"), EventName, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("remove_widget"))
	{
		FString WidgetName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("macro_create_basic_ui_menu"))
	{
		FString MenuTitle;
		TArray<FString> ButtonNames;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("menu_title"), MenuTitle, OutErrors, true) ||
			!UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("button_names"), ButtonNames, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("instantiate_ui_hierarchy"))
	{
		const TArray<TSharedPtr<FJsonValue>>* WidgetsArray = nullptr;
		if (!UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("widgets"), WidgetsArray, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("get_widget_info") || ToolName == TEXT("clear_panel_children"))
	{
		FString WidgetName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, OutErrors, true))
		{
			return false;
		}
	}

	return true;
}

// ============================================================================
// ExecuteAction — Dispatch
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, Result.Errors, false);

	bool bIsReadOnly = (ToolName == TEXT("get_widget_tree") || ToolName == TEXT("capture_widget") || ToolName == TEXT("get_widget_info") || ToolName == TEXT("get_widget_slots"));

	TOptional<FScopedTransaction> Transaction;
	if (!bIsReadOnly)
	{
		Transaction.Emplace(FText::FromString(TEXT("AgentFramework Widget Action")));
	}

	if (ToolName == TEXT("create_widget_blueprint"))       Result = ExecuteCreateWidgetBlueprint(Params, Result);
	else if (ToolName == TEXT("add_widget"))               Result = ExecuteAddWidget(Params, Result);
	else if (ToolName == TEXT("set_widget_slot"))          Result = ExecuteSetWidgetSlot(Params, Result);
	else if (ToolName == TEXT("set_widget_slot_properties")) Result = ExecuteSetWidgetSlotProperties(Params, Result);
	else if (ToolName == TEXT("set_widget_property"))      Result = ExecuteSetWidgetProperty(Params, Result);
	else if (ToolName == TEXT("set_widget_font"))          Result = ExecuteSetWidgetFont(Params, Result);
	else if (ToolName == TEXT("set_widget_brush"))         Result = ExecuteSetWidgetBrush(Params, Result);
	else if (ToolName == TEXT("bind_widget_event"))        Result = ExecuteBindWidgetEvent(Params, Result);
	else if (ToolName == TEXT("remove_widget"))            Result = ExecuteRemoveWidget(Params, Result);
	else if (ToolName == TEXT("get_widget_tree"))          Result = ExecuteGetWidgetTree(Params, Result);
	else if (ToolName == TEXT("compile_widget_blueprint")) Result = ExecuteCompileWidgetBlueprint(Params, Result);
	else if (ToolName == TEXT("macro_create_basic_ui_menu")) Result = ExecuteMacroCreateBasicUIMenu(Params, Result);
	else if (ToolName == TEXT("capture_widget"))           Result = ExecuteCaptureWidget(Params, Result);
	else if (ToolName == TEXT("instantiate_ui_hierarchy")) Result = ExecuteInstantiateUIHierarchy(Params, Result);
	else if (ToolName == TEXT("get_widget_info"))          Result = ExecuteGetWidgetInfo(Params, Result);
	else if (ToolName == TEXT("clear_panel_children"))     Result = ExecuteClearPanelChildren(Params, Result);
	else if (ToolName == TEXT("get_widget_slots"))        Result = ExecuteGetWidgetSlots(Params, Result);
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown Widget tool: '%s'. Supported: create_widget_blueprint, add_widget, set_widget_slot, set_widget_slot_properties, set_widget_property, set_widget_font, set_widget_brush, bind_widget_event, remove_widget, get_widget_tree, compile_widget_blueprint, macro_create_basic_ui_menu, capture_widget, instantiate_ui_hierarchy, get_widget_info, clear_panel_children, get_widget_slots"), *ToolName));
	}

	if (Transaction.IsSet() && !Result.bSuccess)
	{
		Transaction->Cancel();
	}

	return Result;
}

// ============================================================================
// Shared Helpers
// ============================================================================

UClass* FAgentFrameworkWidgetActions::ResolveWidgetClass(const FString& ClassName)
{
	// Panel widgets
	if (ClassName == TEXT("CanvasPanel"))       return UCanvasPanel::StaticClass();
	if (ClassName == TEXT("VerticalBox"))       return UVerticalBox::StaticClass();
	if (ClassName == TEXT("HorizontalBox"))     return UHorizontalBox::StaticClass();
	if (ClassName == TEXT("ScrollBox"))         return UScrollBox::StaticClass();
	if (ClassName == TEXT("Overlay"))           return UOverlay::StaticClass();
	if (ClassName == TEXT("GridPanel"))         return UGridPanel::StaticClass();
	if (ClassName == TEXT("UniformGridPanel"))  return UUniformGridPanel::StaticClass();
	if (ClassName == TEXT("WidgetSwitcher"))    return UWidgetSwitcher::StaticClass();
	if (ClassName == TEXT("WrapBox"))           return UWrapBox::StaticClass();
	if (ClassName == TEXT("MenuAnchor"))        return UMenuAnchor::StaticClass();

	// Content widgets (single child)
	if (ClassName == TEXT("SizeBox"))           return USizeBox::StaticClass();
	if (ClassName == TEXT("ScaleBox"))          return UScaleBox::StaticClass();
	if (ClassName == TEXT("Border"))            return UBorder::StaticClass();
	if (ClassName == TEXT("Button"))            return UButton::StaticClass();
	if (ClassName == TEXT("BackgroundBlur"))    return UBackgroundBlur::StaticClass();
	if (ClassName == TEXT("InvalidationBox"))   return UInvalidationBox::StaticClass();
	if (ClassName == TEXT("RetainerBox"))       return URetainerBox::StaticClass();
	if (ClassName == TEXT("NamedSlot"))         return UNamedSlot::StaticClass();

	// Leaf widgets
	if (ClassName == TEXT("TextBlock"))         return UTextBlock::StaticClass();
	if (ClassName == TEXT("RichTextBlock"))     return URichTextBlock::StaticClass();
	if (ClassName == TEXT("Image"))             return UImage::StaticClass();
	if (ClassName == TEXT("ProgressBar"))       return UProgressBar::StaticClass();
	if (ClassName == TEXT("Slider"))            return USlider::StaticClass();
	if (ClassName == TEXT("CheckBox"))          return UCheckBox::StaticClass();
	if (ClassName == TEXT("EditableTextBox"))   return UEditableTextBox::StaticClass();
	if (ClassName == TEXT("MultiLineEditableTextBox")) return UMultiLineEditableTextBox::StaticClass();
	if (ClassName == TEXT("ComboBoxString"))    return UComboBoxString::StaticClass();
	if (ClassName == TEXT("SpinBox"))           return USpinBox::StaticClass();
	if (ClassName == TEXT("Spacer"))            return USpacer::StaticClass();
	if (ClassName == TEXT("Throbber"))          return UThrobber::StaticClass();
	if (ClassName == TEXT("CircularThrobber"))  return UCircularThrobber::StaticClass();
	if (ClassName == TEXT("ExpandableArea"))    return UExpandableArea::StaticClass();

	// Reflection search — try with and without U prefix
	UClass* Found = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
	if (!IsValid(Found))
		Found = FindFirstObject<UClass>(*(TEXT("U") + ClassName), EFindFirstObjectOptions::None);
	return Found;
}

UWidgetBlueprint* FAgentFrameworkWidgetActions::LoadWidgetBP(const FString& AssetPath, FAgentFrameworkActionResult& Result)
{
	UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
	if (!IsValid(WidgetBP))
	{
		Result.Errors.Add(FString::Printf(TEXT("Widget Blueprint not found: '%s'. Verify the asset exists and the path starts with /Game/."), *AssetPath));
	}
	return WidgetBP;
}

UWidget* FAgentFrameworkWidgetActions::FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FAgentFrameworkActionResult& Result)
{
	if (!IsValid(WidgetBP) || !IsValid(WidgetBP->WidgetTree))
	{
		Result.Errors.Add(TEXT("Widget Blueprint is invalid or has no WidgetTree — recreate the asset."));
		return nullptr;
	}

	UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
	if (!IsValid(Widget))
	{
		// Build helpful list of existing widget names
		TArray<UWidget*> AllWidgets;
		WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
		FString AvailableNames;
		for (UWidget* W : AllWidgets)
		{
			if (IsValid(W))
			{
				if (!AvailableNames.IsEmpty()) AvailableNames += TEXT(", ");
				AvailableNames += W->GetName();
			}
		}
		Result.Errors.Add(FString::Printf(TEXT("Widget '%s' not found. Available widgets: [%s]. Use get_widget_tree to see all widget names."), *WidgetName, *AvailableNames));
	}
	return Widget;
}

bool FAgentFrameworkWidgetActions::ParseVector2D(const FString& Str, FVector2D& OutVec)
{
	TArray<FString> Parts;
	Str.ParseIntoArray(Parts, TEXT(","));
	if (Parts.Num() >= 2)
	{
		OutVec.X = FCString::Atof(*Parts[0].TrimStartAndEnd());
		OutVec.Y = FCString::Atof(*Parts[1].TrimStartAndEnd());
		return true;
	}
	return false;
}

bool FAgentFrameworkWidgetActions::ParseMargin(const FString& Str, FMargin& OutMargin)
{
	TArray<FString> Parts;
	Str.ParseIntoArray(Parts, TEXT(","));
	if (Parts.Num() >= 4)
	{
		OutMargin.Left   = FCString::Atof(*Parts[0].TrimStartAndEnd());
		OutMargin.Top    = FCString::Atof(*Parts[1].TrimStartAndEnd());
		OutMargin.Right  = FCString::Atof(*Parts[2].TrimStartAndEnd());
		OutMargin.Bottom = FCString::Atof(*Parts[3].TrimStartAndEnd());
		return true;
	}
	if (Parts.Num() == 1)
	{
		float Val = FCString::Atof(*Parts[0].TrimStartAndEnd());
		OutMargin = FMargin(Val);
		return true;
	}
	if (Parts.Num() == 2)
	{
		// Horizontal, Vertical
		float H = FCString::Atof(*Parts[0].TrimStartAndEnd());
		float V = FCString::Atof(*Parts[1].TrimStartAndEnd());
		OutMargin = FMargin(H, V, H, V);
		return true;
	}
	return false;
}

EHorizontalAlignment FAgentFrameworkWidgetActions::ParseHAlign(const FString& Str)
{
	if (Str.Equals(TEXT("Left"),   ESearchCase::IgnoreCase)) return HAlign_Left;
	if (Str.Equals(TEXT("Center"), ESearchCase::IgnoreCase)) return HAlign_Center;
	if (Str.Equals(TEXT("Right"),  ESearchCase::IgnoreCase)) return HAlign_Right;
	if (Str.Equals(TEXT("Fill"),   ESearchCase::IgnoreCase)) return HAlign_Fill;
	return HAlign_Fill; // default
}

EVerticalAlignment FAgentFrameworkWidgetActions::ParseVAlign(const FString& Str)
{
	if (Str.Equals(TEXT("Top"),    ESearchCase::IgnoreCase)) return VAlign_Top;
	if (Str.Equals(TEXT("Center"), ESearchCase::IgnoreCase)) return VAlign_Center;
	if (Str.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase)) return VAlign_Bottom;
	if (Str.Equals(TEXT("Fill"),   ESearchCase::IgnoreCase)) return VAlign_Fill;
	return VAlign_Fill; // default
}

bool FAgentFrameworkWidgetActions::ParseLinearColor(const FString& Str, FLinearColor& OutColor)
{
	// Accept format "(R=1.0,G=0.5,B=0.0,A=1.0)"
	FString Clean = Str;
	Clean.ReplaceInline(TEXT("("), TEXT(""));
	Clean.ReplaceInline(TEXT(")"), TEXT(""));

	TMap<FString, float> Values;
	TArray<FString> Parts;
	Clean.ParseIntoArray(Parts, TEXT(","));
	for (const FString& Part : Parts)
	{
		FString Key, Val;
		if (Part.Split(TEXT("="), &Key, &Val))
		{
			Values.Add(Key.TrimStartAndEnd(), FCString::Atof(*Val.TrimStartAndEnd()));
		}
	}

	if (Values.Contains(TEXT("R")))
	{
		OutColor.R = Values.FindRef(TEXT("R"));
		OutColor.G = Values.FindRef(TEXT("G"));
		OutColor.B = Values.FindRef(TEXT("B"));
		OutColor.A = Values.Contains(TEXT("A")) ? Values.FindRef(TEXT("A")) : 1.0f;
		return true;
	}
	return false;
}

void FAgentFrameworkWidgetActions::CompileAndMarkDirty(UWidgetBlueprint* WidgetBP)
{
	if (!IsValid(WidgetBP)) return;
	FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::SkipGarbageCollection);
	if (IsValid(WidgetBP->GetOutermost()))
	{
		WidgetBP->GetOutermost()->MarkPackageDirty();
	}
}

// ============================================================================
// BuildWidgetTreeJson
// ============================================================================

FString FAgentFrameworkWidgetActions::BuildWidgetTreeJson(UWidgetBlueprint* WidgetBlueprint)
{
	if (!IsValid(WidgetBlueprint)) return TEXT("{}");

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), WidgetBlueprint->GetPathName());
	Root->SetStringField(TEXT("parent_class"), IsValid(WidgetBlueprint->ParentClass) ? WidgetBlueprint->ParentClass->GetName() : TEXT("UserWidget"));

	TArray<TSharedPtr<FJsonValue>> WidgetsArray;

	if (IsValid(WidgetBlueprint->WidgetTree))
	{
		WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (!IsValid(Widget)) return;
			TSharedPtr<FJsonObject> WObj = MakeShared<FJsonObject>();
			WObj->SetStringField(TEXT("name"), Widget->GetName());
			WObj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());

			// Is this widget the root?
			WObj->SetBoolField(TEXT("is_root"), Widget == WidgetBlueprint->WidgetTree->RootWidget);

			// Parent name
			UPanelWidget* ParentPanel = Widget->GetParent();
			if (IsValid(ParentPanel))
			{
				WObj->SetStringField(TEXT("parent"), ParentPanel->GetName());
			}
			else if (Widget != WidgetBlueprint->WidgetTree->RootWidget)
			{
				WObj->SetStringField(TEXT("parent"), TEXT("(orphaned)"));
			}

			// Is it a panel (can contain children)?
			UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
			if (IsValid(Panel))
			{
				WObj->SetBoolField(TEXT("is_panel"), true);
				WObj->SetNumberField(TEXT("child_count"), Panel->GetChildrenCount());
			}
			else
			{
				WObj->SetBoolField(TEXT("is_panel"), false);
			}

			// Slot type
			if (IsValid(Widget->Slot))
			{
				WObj->SetStringField(TEXT("slot_type"), Widget->Slot->GetClass()->GetName());
			}

			WidgetsArray.Add(MakeShared<FJsonValueObject>(WObj));
		});
	}

	Root->SetArrayField(TEXT("widgets"), WidgetsArray);

	int32 WidgetCount = WidgetsArray.Num();
	Root->SetNumberField(TEXT("total_widget_count"), WidgetCount);

	FString OutputStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputStr);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return OutputStr;
}

// ============================================================================
// ExecuteCreateWidgetBlueprint
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteCreateWidgetBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	if (!AssetPath.StartsWith(TEXT("/Game/")))
	{
		Result.Errors.Add(FString::Printf(TEXT("asset_path '%s' must start with /Game/. Example: /Game/UI/WBP_MainMenu"), *AssetPath));
		return Result;
	}

	FString RootWidgetClassName = TEXT("CanvasPanel");
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("root_widget_class"), RootWidgetClassName, Result.Errors, false);

	FString ParentClassName = TEXT("UserWidget");
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("parent_class"), ParentClassName, Result.Errors, false);

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName   = FPackageName::GetShortName(AssetPath);

	// Resolve parent class
	UClass* ParentClass = UUserWidget::StaticClass();
	if (!ParentClassName.IsEmpty() && ParentClassName != TEXT("UserWidget"))
	{
		UClass* FoundClass = FindFirstObject<UClass>(*ParentClassName, EFindFirstObjectOptions::None);
		if (!IsValid(FoundClass))
			FoundClass = FindFirstObject<UClass>(*(TEXT("U") + ParentClassName), EFindFirstObjectOptions::None);
		if (IsValid(FoundClass) && FoundClass->IsChildOf(UUserWidget::StaticClass()))
		{
			ParentClass = FoundClass;
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Parent class '%s' not found or not a UUserWidget subclass. Using default UUserWidget."), *ParentClassName));
		}
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->ParentClass = ParentClass;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory);
	UWidgetBlueprint* NewWidget = Cast<UWidgetBlueprint>(NewAsset);

	if (!IsValid(NewWidget))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Widget Blueprint at '%s'. Verify the path is valid, starts with /Game/, and the directory exists. Example: /Game/UI/WBP_MainMenu"), *AssetPath));
		return Result;
	}

	// Set the root widget
	if (IsValid(NewWidget->WidgetTree) && !RootWidgetClassName.IsEmpty() && RootWidgetClassName != TEXT("none"))
	{
		UClass* RootClass = ResolveWidgetClass(RootWidgetClassName);
		if (IsValid(RootClass))
		{
			NewWidget->Modify();
			NewWidget->WidgetTree->Modify();
			UWidget* RootWidget = NewWidget->WidgetTree->ConstructWidget<UWidget>(RootClass, FName(*RootWidgetClassName));
			if (IsValid(RootWidget))
			{
				NewWidget->WidgetVariableNameToGuidMap.Add(RootWidget->GetFName(), FGuid::NewGuid());
				NewWidget->WidgetTree->RootWidget = RootWidget;
				UE_LOG(LogAgentFramework, Log, TEXT("WidgetActions: Set root widget '%s' on '%s'"), *RootWidgetClassName, *AssetName);
			}
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Root widget class '%s' not found — Widget Blueprint created without a root widget. Valid classes: CanvasPanel, VerticalBox, HorizontalBox, SizeBox, Overlay, GridPanel, ScrollBox, WrapBox."), *RootWidgetClassName));
		}
	}

	// Compile
	FKismetEditorUtilities::CompileBlueprint(NewWidget, EBlueprintCompileOptions::SkipGarbageCollection);

	// Save
	UPackage* Package = NewWidget->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, NewWidget, *PackageFilename, SaveArgs);
		}
	}

	FAssetRegistryModule::AssetCreated(NewWidget);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created Widget Blueprint '%s' (parent: %s) with root widget '%s'. Next: use add_widget to populate the tree, then set_widget_slot to configure layout."), *AssetName, *ParentClass->GetName(), *RootWidgetClassName);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteMacroCreateBasicUIMenu
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteMacroCreateBasicUIMenu(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath, MenuTitle;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("menu_title"), MenuTitle, Result.Errors, true))
	{
		return Result;
	}

	TArray<FString> ButtonNames;
	if (!UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("button_names"), ButtonNames, Result.Errors, true))
	{
		return Result;
	}

	// 1. Create the base Widget Blueprint with a CanvasPanel root
	TSharedPtr<FJsonObject> CreateParams = MakeShared<FJsonObject>();
	CreateParams->SetStringField(TEXT("asset_path"), AssetPath);
	CreateParams->SetStringField(TEXT("root_widget_class"), TEXT("CanvasPanel"));

	FAgentFrameworkActionResult CreateResult;
	ExecuteCreateWidgetBlueprint(CreateParams.ToSharedRef(), CreateResult);
	if (!CreateResult.bSuccess)
	{
		Result.Errors.Append(CreateResult.Errors);
		return Result;
	}

	// 2. Load the newly created asset
	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP) || !IsValid(WidgetBP->WidgetTree)) return Result;

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBP->WidgetTree->RootWidget);
	if (!IsValid(RootCanvas))
	{
		Result.Errors.Add(TEXT("Root widget is not a CanvasPanel."));
		return Result;
	}

	// 3. Create Vertical Box and add to Canvas
	UVerticalBox* VBox = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuVerticalBox"));
	if (IsValid(VBox))
	{
		WidgetBP->WidgetVariableNameToGuidMap.Add(VBox->GetFName(), FGuid::NewGuid());
	}
	UCanvasPanelSlot* VBoxSlot = Cast<UCanvasPanelSlot>(RootCanvas->AddChild(VBox));
	if (IsValid(VBoxSlot))
	{
		FAnchors CenterAnchor(0.5f, 0.5f, 0.5f, 0.5f);
		VBoxSlot->SetAnchors(CenterAnchor);
		VBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		VBoxSlot->SetAutoSize(true);
	}

	// 4. Create Title Text
	UTextBlock* TitleText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	if (IsValid(TitleText))
	{
		WidgetBP->WidgetVariableNameToGuidMap.Add(TitleText->GetFName(), FGuid::NewGuid());
		TitleText->SetText(FText::FromString(MenuTitle));
		UVerticalBoxSlot* TitleSlot = Cast<UVerticalBoxSlot>(VBox->AddChild(TitleText));
		if (IsValid(TitleSlot))
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
			TitleSlot->SetPadding(FMargin(0, 0, 0, 40));
		}
	}

	// 5. Create Buttons
	for (const FString& BtnName : ButtonNames)
	{
		FString BtnWidgetName = BtnName + TEXT("Button");
		BtnWidgetName.ReplaceInline(TEXT(" "), TEXT(""));

		UButton* Btn = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*BtnWidgetName));
		if (IsValid(Btn))
		{
			WidgetBP->WidgetVariableNameToGuidMap.Add(Btn->GetFName(), FGuid::NewGuid());
			UVerticalBoxSlot* BtnSlot = Cast<UVerticalBoxSlot>(VBox->AddChild(Btn));
			if (IsValid(BtnSlot))
			{
				BtnSlot->SetPadding(FMargin(0, 0, 0, 10));
				BtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}

			UTextBlock* BtnText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*(BtnWidgetName + TEXT("Text"))));
			if (IsValid(BtnText))
			{
				WidgetBP->WidgetVariableNameToGuidMap.Add(BtnText->GetFName(), FGuid::NewGuid());
				BtnText->SetText(FText::FromString(BtnName));
				BtnText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
				Btn->AddChild(BtnText);
			}

			// Bind empty click event via K2Node
			if (UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(WidgetBP))
			{
				FMulticastDelegateProperty* DelegateProp = CastField<FMulticastDelegateProperty>(UButton::StaticClass()->FindPropertyByName(TEXT("OnClicked")));
				if (DelegateProp)
				{
					UK2Node_ComponentBoundEvent* EventNode = NewObject<UK2Node_ComponentBoundEvent>(EventGraph);
					if (IsValid(EventNode))
					{
						EventNode->DelegatePropertyName = DelegateProp->GetFName();
						EventNode->DelegateOwnerClass = UButton::StaticClass();
						EventNode->ComponentPropertyName = FName(*BtnWidgetName);
						
						EventNode->NodePosX = 0;
						EventNode->NodePosY = 150 * ButtonNames.IndexOfByKey(BtnName); // space them out
						
						EventGraph->AddNode(EventNode, false, false);
						EventNode->CreateNewGuid();
						EventNode->PostPlacedNewNode();
						EventNode->AllocateDefaultPins();
					}
				}
			}
		}
	}

	CompileAndMarkDirty(WidgetBP);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully created UI Menu '%s' with %d buttons."), *AssetPath, ButtonNames.Num());
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteAddWidget
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteAddWidget(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath, WidgetClassName, WidgetName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_class"), WidgetClassName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	if (!IsValid(WidgetBP->WidgetTree))
	{
		Result.Errors.Add(TEXT("Widget Blueprint has no WidgetTree — recreate the asset."));
		return Result;
	}

	UClass* WidgetClass = ResolveWidgetClass(WidgetClassName);
	if (!IsValid(WidgetClass))
	{
		Result.Errors.Add(FString::Printf(TEXT("Widget class not found: '%s'. Valid classes: CanvasPanel, VerticalBox, HorizontalBox, TextBlock, Button, Image, ScrollBox, SizeBox, Overlay, WidgetSwitcher, ProgressBar, Slider, CheckBox, EditableTextBox, MultiLineEditableTextBox, ComboBoxString, SpinBox, Spacer, Border, BackgroundBlur, WrapBox, ScaleBox, RichTextBlock, Throbber, CircularThrobber, ExpandableArea, GridPanel, UniformGridPanel"), *WidgetClassName));
		return Result;
	}

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();

	UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
	if (!IsValid(NewWidget))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to construct widget of class '%s'."), *WidgetClassName));
		return Result;
	}

	WidgetBP->WidgetVariableNameToGuidMap.Add(NewWidget->GetFName(), FGuid::NewGuid());

	// Attach to parent panel if specified
	FString ParentWidgetName;
	bool bAddedToParent = false;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("parent_widget"), ParentWidgetName, Result.Errors, false) && !ParentWidgetName.IsEmpty())
	{
		UWidget* ParentWidgetRaw = WidgetBP->WidgetTree->FindWidget(FName(*ParentWidgetName));
		UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidgetRaw);
		if (IsValid(ParentPanel))
		{
			UPanelSlot* Slot = ParentPanel->AddChild(NewWidget);
			bAddedToParent = true;

			// Report the slot type so the AI knows what properties are available
			if (IsValid(Slot))
			{
				FString SlotType = Slot->GetClass()->GetName();
				Result.Warnings.Add(FString::Printf(TEXT("Widget added to '%s'. Slot type: %s. Use set_widget_slot to configure layout (anchors, padding, alignment)."), *ParentWidgetName, *SlotType));
			}
		}
		else if (IsValid(ParentWidgetRaw))
		{
			// Check if it's a content widget (single child) — like Button, SizeBox, Border
			UContentWidget* ContentParent = Cast<UContentWidget>(ParentWidgetRaw);
			if (IsValid(ContentParent))
			{
				if (ContentParent->GetChildrenCount() > 0)
				{
					Result.Warnings.Add(FString::Printf(TEXT("Content widget '%s' already has a child. Replacing it."), *ParentWidgetName));
					ContentParent->ClearChildren();
				}
				ContentParent->AddChild(NewWidget);
				bAddedToParent = true;
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Parent widget '%s' (%s) is not a panel or content widget. Supported parents: CanvasPanel, VerticalBox, HorizontalBox, ScrollBox, GridPanel, Overlay, WidgetSwitcher, SizeBox, Border, Button, WrapBox, ScaleBox, BackgroundBlur."), *ParentWidgetName, *ParentWidgetRaw->GetClass()->GetName()));
			}
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Parent widget '%s' not found in widget tree. Use get_widget_tree to see all widget names."), *ParentWidgetName));
		}
	}

	// If not added to a parent and no root exists, set as root
	if (!bAddedToParent && !IsValid(WidgetBP->WidgetTree->RootWidget))
	{
		WidgetBP->WidgetTree->RootWidget = NewWidget;
		Result.Warnings.Add(FString::Printf(TEXT("No parent_widget specified and no root exists — '%s' set as root widget."), *WidgetName));
	}
	else if (!bAddedToParent)
	{
		// Try to attach to root if it's a panel
		UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
		if (IsValid(RootPanel))
		{
			RootPanel->AddChild(NewWidget);
			Result.Warnings.Add(FString::Printf(TEXT("No parent_widget specified — attached '%s' to root panel '%s' by default. Use set_widget_slot to configure layout."), *WidgetName, *RootPanel->GetName()));
		}
		else
		{
			Result.Warnings.Add(TEXT("Could not attach widget — specify parent_widget or set a panel as the root first."));
		}
	}

	CompileAndMarkDirty(WidgetBP);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Added widget '%s' (%s) to '%s'. IMPORTANT: If the parent is a CanvasPanel, you MUST call set_widget_slot to set anchors/position/size — otherwise the widget will be invisible (zero size at 0,0)."), *WidgetName, *WidgetClassName, *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ============================================================================
// ExecuteSetWidgetSlot & ExecuteSetWidgetSlotProperties
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteSetWidgetSlotProperties(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	return ExecuteSetWidgetSlot(Params, Result);
}

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteSetWidgetSlot(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath, WidgetName;
	if (!ResolveWidgetAssetPath(Params, AssetPath, Result.Errors) ||
		!ResolveWidgetName(Params, WidgetName, Result.Errors))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!IsValid(Widget)) return Result;

	if (!IsValid(Widget->Slot))
	{
		Result.Errors.Add(FString::Printf(TEXT("Widget '%s' has no slot. It may be the root widget (root widgets don't have slots) or not yet attached to a parent panel. Add it to a panel first via add_widget."), *WidgetName));
		return Result;
	}

	WidgetBP->Modify();
	Widget->Slot->Modify();

	if (ApplyWidgetSlotHelper(WidgetBP, Widget, Params, Result))
	{
		CompileAndMarkDirty(WidgetBP);
		Result.bSuccess = true;
		Result.ModifiedAssets.Add(AssetPath);
	}

	return Result;
}

// ============================================================================
// ExecuteSetWidgetProperty
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteSetWidgetProperty(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath, WidgetName, PropertyName, PropertyValue;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("property_name"), PropertyName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("property_value"), PropertyValue, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	UWidget* TargetWidget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!IsValid(TargetWidget)) return Result;

	WidgetBP->Modify();

	if (ApplyWidgetPropertyHelper(WidgetBP, TargetWidget, PropertyName, PropertyValue, Result))
	{
		CompileAndMarkDirty(WidgetBP);
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Set '%s.%s' = '%s' in '%s'."), *WidgetName, *PropertyName, *PropertyValue, *AssetPath);
		Result.ModifiedAssets.Add(AssetPath);
	}
	return Result;
}

// ============================================================================
// ExecuteSetWidgetFont
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteSetWidgetFont(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath, WidgetName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!IsValid(Widget)) return Result;

	WidgetBP->Modify();
	Widget->Modify();

	if (ApplyWidgetFontHelper(WidgetBP, Widget, Params, Result))
	{
		CompileAndMarkDirty(WidgetBP);

		FSlateFontInfo FontInfo;
		UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
		if (IsValid(TextBlock))
		{
			FontInfo = TextBlock->GetFont();
		}
		else
		{
			FProperty* FontProp = Widget->GetClass()->FindPropertyByName(FName("Font"));
			if (FontProp)
			{
				FSlateFontInfo* FontPtr = FontProp->ContainerPtrToValuePtr<FSlateFontInfo>(Widget);
				if (FontPtr)
				{
					FontInfo = *FontPtr;
				}
			}
		}

		FString FontFamily, Typeface;
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("font_family"), FontFamily, Result.Errors, false);
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("typeface"), Typeface, Result.Errors, false);

		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Set font on '%s': size=%d%s%s."),
			*WidgetName, static_cast<int32>(FontInfo.Size),
			!Typeface.IsEmpty() ? *FString::Printf(TEXT(", typeface=%s"), *Typeface) : TEXT(""),
			!FontFamily.IsEmpty() ? *FString::Printf(TEXT(", family=%s"), *FontFamily) : TEXT(""));
		Result.ModifiedAssets.Add(AssetPath);
	}
	return Result;
}

// ============================================================================
// ExecuteSetWidgetBrush
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteSetWidgetBrush(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath, WidgetName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!IsValid(Widget)) return Result;

	WidgetBP->Modify();
	Widget->Modify();

	if (ApplyWidgetBrushHelper(WidgetBP, Widget, Params, Result))
	{
		CompileAndMarkDirty(WidgetBP);

		FString BrushTarget = TEXT("Brush");
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("brush_target"), BrushTarget, Result.Errors, false);
		FString TexturePath;
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("texture_path"), TexturePath, Result.Errors, false);

		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Set brush '%s' on '%s' in '%s'.%s"),
			*BrushTarget, *WidgetName, *AssetPath,
			TexturePath.IsEmpty() ? TEXT(" (solid color brush)") : *FString::Printf(TEXT(" (texture: %s)"), *TexturePath));
		Result.ModifiedAssets.Add(AssetPath);
	}
	return Result;
}

// ============================================================================
// ExecuteBindWidgetEvent
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteBindWidgetEvent(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath, WidgetName, EventName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("event_name"), EventName, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!IsValid(Widget)) return Result;

	WidgetBP->Modify();

	if (ApplyWidgetEventHelper(WidgetBP, Widget, EventName, TEXT(""), Result))
	{
		CompileAndMarkDirty(WidgetBP);

		FString NodeName;
		int32 NodePosX = 0, NodePosY = 0;
		UEdGraph* EventGraph = nullptr;
		for (UEdGraph* Graph : WidgetBP->UbergraphPages)
		{
			if (IsValid(Graph) && Graph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				EventGraph = Graph;
				break;
			}
		}
		if (!IsValid(EventGraph) && WidgetBP->UbergraphPages.Num() > 0)
		{
			EventGraph = WidgetBP->UbergraphPages[0];
		}
		if (IsValid(EventGraph))
		{
			for (UEdGraphNode* Node : EventGraph->Nodes)
			{
				UK2Node_ComponentBoundEvent* BoundNode = Cast<UK2Node_ComponentBoundEvent>(Node);
				if (IsValid(BoundNode) && BoundNode->ComponentPropertyName == FName(*WidgetName) && BoundNode->DelegatePropertyName == FName(*EventName))
				{
					NodeName = BoundNode->GetName();
					NodePosX = BoundNode->NodePosX;
					NodePosY = BoundNode->NodePosY;
				}
			}
		}

		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(
			TEXT("Bound event '%s' on widget '%s' in '%s'. Created event node '%s' at position (%d, %d). Use inject_blueprint_nodes_t3d to wire logic from this event node's exec output pin."),
			*EventName, *WidgetName, *AssetPath, *NodeName, NodePosX, NodePosY);
		Result.ModifiedAssets.Add(AssetPath);
	}
	return Result;
}

// ============================================================================
// ExecuteRemoveWidget
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteRemoveWidget(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath, WidgetName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!IsValid(Widget)) return Result;

	WidgetBP->Modify();
	if (IsValid(WidgetBP->WidgetTree))
	{
		WidgetBP->WidgetTree->Modify();
	}

	bool bWasRoot = (IsValid(WidgetBP->WidgetTree) && Widget == WidgetBP->WidgetTree->RootWidget);

	// If the widget has a parent panel, remove from it
	UPanelWidget* Parent = Widget->GetParent();
	if (IsValid(Parent))
	{
		Parent->Modify();
		Parent->RemoveChild(Widget);
	}

	// If it was the root, clear the root
	if (bWasRoot && IsValid(WidgetBP->WidgetTree))
	{
		WidgetBP->WidgetTree->RootWidget = nullptr;
		Result.Warnings.Add(TEXT("Removed the root widget. The widget tree is now empty. Use add_widget to add a new root."));
	}

	CompileAndMarkDirty(WidgetBP);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Removed widget '%s' from '%s'.%s"), *WidgetName, *AssetPath,
		bWasRoot ? TEXT(" (was root — tree is now empty)") : TEXT(""));
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteGetWidgetTree
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteGetWidgetTree(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	FString Json = BuildWidgetTreeJson(WidgetBP);
	Result.bSuccess = true;
	Result.ResultMessage = Json;
	return Result;
}

// ============================================================================
// ExecuteCompileWidgetBlueprint
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteCompileWidgetBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	FCompilerResultsLog Log;
	FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::None, &Log);

	for (const TSharedRef<FTokenizedMessage>& Msg : Log.Messages)
	{
		FString MsgText = Msg->ToText().ToString();
		if (Msg->GetSeverity() == EMessageSeverity::Error)
			Result.Errors.Add(FString::Printf(TEXT("COMPILE ERROR: %s"), *MsgText));
		else if (Msg->GetSeverity() == EMessageSeverity::Warning)
			Result.Warnings.Add(FString::Printf(TEXT("COMPILE WARNING: %s"), *MsgText));
	}

	bool bOk = (WidgetBP->Status != BS_Error);

	// Add widget tree summary to the result
	int32 WidgetCount = 0;
	FString RootName = TEXT("(none)");
	if (IsValid(WidgetBP->WidgetTree))
	{
		TArray<UWidget*> AllWidgets;
		WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
		WidgetCount = AllWidgets.Num();
		if (IsValid(WidgetBP->WidgetTree->RootWidget))
		{
			RootName = FString::Printf(TEXT("%s (%s)"), *WidgetBP->WidgetTree->RootWidget->GetName(), *WidgetBP->WidgetTree->RootWidget->GetClass()->GetName());
		}
	}

	Result.bSuccess = bOk;
	Result.ResultMessage = bOk
		? FString::Printf(TEXT("Widget Blueprint '%s' compiled successfully. %d widgets, root: %s."), *AssetPath, WidgetCount, *RootName)
		: FString::Printf(TEXT("Widget Blueprint '%s' compiled with ERRORS. %d widgets, root: %s."), *AssetPath, WidgetCount, *RootName);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteCaptureWidget
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteCaptureWidget(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	UClass* WidgetClass = WidgetBP->GeneratedClass;
	if (!IsValid(WidgetClass))
	{
		Result.Errors.Add(TEXT("Widget Blueprint has no GeneratedClass. Try compiling it first."));
		return Result;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("Could not find a valid World context to spawn the widget."));
		return Result;
	}

	UUserWidget* CreatedWidget = CreateWidget<UUserWidget>(World, WidgetClass);
	if (!IsValid(CreatedWidget))
	{
		Result.Errors.Add(TEXT("Failed to create temporary widget instance."));
		return Result;
	}

	TSharedRef<SWidget> SlateWidget = CreatedWidget->TakeWidget();

	// Measure desired size
	SlateWidget->SlatePrepass(1.0f);
	FVector2D DesiredSize = SlateWidget->GetDesiredSize();

	// Determine render size
	FVector2D RenderSize = DesiredSize;

	FString RenderSizeStr;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("render_size"), RenderSizeStr, Result.Errors, false) && !RenderSizeStr.IsEmpty())
	{
		if (!ParseVector2D(RenderSizeStr, RenderSize))
		{
			Result.Errors.Add(FString::Printf(TEXT("Invalid render_size format: '%s'. Expected 'X,Y'"), *RenderSizeStr));
			return Result;
		}
	}
	else if (RenderSize.X <= 0 || RenderSize.Y <= 0)
	{
		FString ResolutionStr = TEXT("1920,1080");
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("resolution"), ResolutionStr, Result.Errors, false);
		if (!ParseVector2D(ResolutionStr, RenderSize))
		{
			RenderSize = FVector2D(1920, 1080);
		}
	}

	int32 Width = FMath::Max(1, FMath::RoundToInt(RenderSize.X));
	int32 Height = FMath::Max(1, FMath::RoundToInt(RenderSize.Y));

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	if (!IsValid(RenderTarget))
	{
		Result.Errors.Add(TEXT("Failed to create RenderTarget for widget capture."));
		return Result;
	}
	RenderTarget->InitCustomFormat(Width, Height, PF_B8G8R8A8, false);

	FWidgetRenderer* WidgetRenderer = new FWidgetRenderer(true);
	WidgetRenderer->DrawWidget(RenderTarget, SlateWidget, FVector2D(Width, Height), 0.0f);
	FlushRenderingCommands();

	FRenderTarget* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		delete WidgetRenderer;
		Result.Errors.Add(TEXT("Failed to get render target resource."));
		return Result;
	}

	TArray<FColor> Pixels;
	if (!RTResource->ReadPixels(Pixels))
	{
		delete WidgetRenderer;
		Result.Errors.Add(TEXT("Failed to read pixels from render target."));
		return Result;
	}

	delete WidgetRenderer;

	int32 MaxDimension = 512;
	UAgentFrameworkActionUtils::TryGetIntParam(Params, TEXT("max_dimension"), MaxDimension, Result.Errors, false);

	int32 Quality = 75;
	UAgentFrameworkActionUtils::TryGetIntParam(Params, TEXT("quality"), Quality, Result.Errors, false);

	FString FilePath = FAgentFrameworkViewportActions::SavePixelsToDisk(Pixels, Width, Height, MaxDimension, Quality);
	if (FilePath.IsEmpty())
	{
		Result.Errors.Add(TEXT("Failed to save widget capture to disk."));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(
		TEXT("[IMAGE:%s]\n\n"
			 "Widget '%s' captured successfully (rendered at %dx%d, resized to max %dpx, JPEG quality %d). "
			 "The image has been saved to the path above."),
		*FilePath, *AssetPath, Width, Height, MaxDimension, Quality);

	return Result;
}

// ============================================================================
// Helpers & Batch Instantiation
// ============================================================================

bool FAgentFrameworkWidgetActions::ApplyWidgetPropertyHelper(UWidgetBlueprint* WidgetBP, UWidget* TargetWidget, const FString& PropertyName, const FString& PropertyValue, FAgentFrameworkActionResult& Result)
{
	if (!IsValid(WidgetBP) || !IsValid(TargetWidget)) return false;

	FString WidgetName = TargetWidget->GetName();

	// Special handling for Text property on TextBlock — use SetText for proper FText
	if (PropertyName == TEXT("Text"))
	{
		UTextBlock* TextBlock = Cast<UTextBlock>(TargetWidget);
		if (IsValid(TextBlock))
		{
			TextBlock->Modify();
			TextBlock->SetText(FText::FromString(PropertyValue));
			return true;
		}

		// Also handle EditableTextBox Text
		UEditableTextBox* EditBox = Cast<UEditableTextBox>(TargetWidget);
		if (IsValid(EditBox))
		{
			EditBox->Modify();
			EditBox->SetText(FText::FromString(PropertyValue));
			return true;
		}
	}

	// Special handling for HintText on EditableTextBox
	if (PropertyName == TEXT("HintText"))
	{
		UEditableTextBox* EditBox = Cast<UEditableTextBox>(TargetWidget);
		if (IsValid(EditBox))
		{
			EditBox->Modify();
			EditBox->SetHintText(FText::FromString(PropertyValue));
			return true;
		}
	}

	// Generic reflection path
	FProperty* Prop = TargetWidget->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop)
	{
		Result.Errors.Add(FString::Printf(TEXT("Property '%s' not found on widget '%s' (%s). Property names are exact C++ names (case-sensitive). Common properties: Text, ColorAndOpacity, Font, Justification, Visibility, Padding, bIsEnabled, BackgroundColor, Brush, Percent, Value, IsFocusable, RenderOpacity, Clipping"), *PropertyName, *WidgetName, *TargetWidget->GetClass()->GetName()));
		return false;
	}

	TargetWidget->Modify();
	void* PropAddr = Prop->ContainerPtrToValuePtr<void>(TargetWidget);
	Prop->ImportText_Direct(*PropertyValue, PropAddr, TargetWidget, PPF_None);

	UE_LOG(LogAgentFramework, Log, TEXT("WidgetActions: Set property '%s' = '%s' on widget '%s'"), *PropertyName, *PropertyValue, *WidgetName);

	return true;
}

bool FAgentFrameworkWidgetActions::ApplyWidgetFontHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& FontParams, FAgentFrameworkActionResult& Result)
{
	if (!IsValid(WidgetBP) || !IsValid(Widget) || !FontParams.IsValid()) return false;

	FString WidgetName = Widget->GetName();

	// Get the current font info from the widget
	FSlateFontInfo FontInfo;

	UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
	if (IsValid(TextBlock))
	{
		FontInfo = TextBlock->GetFont();
	}
	else
	{
		// Try to get Font property via reflection
		FProperty* FontProp = Widget->GetClass()->FindPropertyByName(FName("Font"));
		if (FontProp)
		{
			FSlateFontInfo* FontPtr = FontProp->ContainerPtrToValuePtr<FSlateFontInfo>(Widget);
			if (FontPtr)
			{
				FontInfo = *FontPtr;
			}
		}
		else
		{
			Result.Errors.Add(FString::Printf(TEXT("Widget '%s' (%s) does not have a Font property. set_widget_font only works on text widgets: TextBlock, EditableTextBox, RichTextBlock, MultiLineEditableTextBox."), *WidgetName, *Widget->GetClass()->GetName()));
			return false;
		}
	}

	// Apply font family
	FString FontFamily;
	if (UAgentFrameworkActionUtils::TryGetStringParam(FontParams, TEXT("font_family"), FontFamily, Result.Errors, false) && !FontFamily.IsEmpty())
	{
		if (FontFamily.Equals(TEXT("Roboto"), ESearchCase::IgnoreCase))
		{
			UObject* FontObj = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
			if (IsValid(FontObj))
			{
				FontInfo.FontObject = FontObj;
			}
		}
		else if (FontFamily.Equals(TEXT("DroidSansMono"), ESearchCase::IgnoreCase))
		{
			UObject* FontObj = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono"));
			if (IsValid(FontObj))
			{
				FontInfo.FontObject = FontObj;
			}
		}
		else if (FontFamily.StartsWith(TEXT("/Game/")) || FontFamily.StartsWith(TEXT("/Engine/")))
		{
			UObject* FontObj = LoadObject<UFont>(nullptr, *FontFamily);
			if (IsValid(FontObj))
			{
				FontInfo.FontObject = FontObj;
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Font asset '%s' not found. Using default Roboto."), *FontFamily));
			}
		}
	}

	// Apply font size
	int32 FontSize = 12;
	if (UAgentFrameworkActionUtils::TryGetIntParam(FontParams, TEXT("font_size"), FontSize, Result.Errors, false))
	{
		FontInfo.Size = FontSize;
	}

	// Apply typeface
	FString Typeface;
	if (UAgentFrameworkActionUtils::TryGetStringParam(FontParams, TEXT("typeface"), Typeface, Result.Errors, false) && !Typeface.IsEmpty())
	{
		FontInfo.TypefaceFontName = FName(*Typeface);
	}

	// Set the font back on the widget
	if (IsValid(TextBlock))
	{
		TextBlock->SetFont(FontInfo);
	}
	else
	{
		FProperty* FontProp = Widget->GetClass()->FindPropertyByName(FName("Font"));
		if (FontProp)
		{
			FSlateFontInfo* FontPtr = FontProp->ContainerPtrToValuePtr<FSlateFontInfo>(Widget);
			if (FontPtr)
			{
				*FontPtr = FontInfo;
			}
		}
	}

	// Apply color (TextBlock only)
	FString ColorStr;
	if (IsValid(TextBlock) && UAgentFrameworkActionUtils::TryGetStringParam(FontParams, TEXT("color"), ColorStr, Result.Errors, false))
	{
		FLinearColor Color;
		if (ParseLinearColor(ColorStr, Color))
		{
			TextBlock->SetColorAndOpacity(FSlateColor(Color));
		}
	}

	// Apply shadow
	FString ShadowOffsetStr;
	if (IsValid(TextBlock) && UAgentFrameworkActionUtils::TryGetStringParam(FontParams, TEXT("shadow_offset"), ShadowOffsetStr, Result.Errors, false))
	{
		FVector2D ShadowOffset;
		if (ParseVector2D(ShadowOffsetStr, ShadowOffset))
		{
			TextBlock->SetShadowOffset(ShadowOffset);
		}
	}

	FString ShadowColorStr;
	if (IsValid(TextBlock) && UAgentFrameworkActionUtils::TryGetStringParam(FontParams, TEXT("shadow_color"), ShadowColorStr, Result.Errors, false))
	{
		FLinearColor ShadowColor;
		if (ParseLinearColor(ShadowColorStr, ShadowColor))
		{
			TextBlock->SetShadowColorAndOpacity(ShadowColor);
		}
	}

	return true;
}

bool FAgentFrameworkWidgetActions::ApplyWidgetBrushHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& BrushParams, FAgentFrameworkActionResult& Result)
{
	if (!IsValid(WidgetBP) || !IsValid(Widget) || !BrushParams.IsValid()) return false;

	FString WidgetName = Widget->GetName();

	FString BrushTarget = TEXT("Brush");
	UAgentFrameworkActionUtils::TryGetStringParam(BrushParams, TEXT("brush_target"), BrushTarget, Result.Errors, false);

	// Build the brush
	FSlateBrush NewBrush;

	// Load texture if provided
	FString TexturePath;
	if (UAgentFrameworkActionUtils::TryGetStringParam(BrushParams, TEXT("texture_path"), TexturePath, Result.Errors, false) && !TexturePath.IsEmpty())
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
		if (IsValid(Texture))
		{
			NewBrush.SetResourceObject(Texture);
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Texture '%s' not found. Creating solid color brush instead."), *TexturePath));
		}
	}

	// Set tint color
	FString TintColorStr;
	if (UAgentFrameworkActionUtils::TryGetStringParam(BrushParams, TEXT("tint_color"), TintColorStr, Result.Errors, false))
	{
		FLinearColor TintColor;
		if (ParseLinearColor(TintColorStr, TintColor))
		{
			NewBrush.TintColor = FSlateColor(TintColor);
		}
	}

	// Set image size
	FString ImageSizeStr;
	if (UAgentFrameworkActionUtils::TryGetStringParam(BrushParams, TEXT("image_size"), ImageSizeStr, Result.Errors, false))
	{
		FVector2D ImageSize;
		FString Clean = ImageSizeStr;
		Clean.ReplaceInline(TEXT("("), TEXT(""));
		Clean.ReplaceInline(TEXT(")"), TEXT(""));
		Clean.ReplaceInline(TEXT("X="), TEXT(""));
		Clean.ReplaceInline(TEXT("Y="), TEXT(""));
		if (ParseVector2D(Clean, ImageSize))
		{
			NewBrush.ImageSize = ImageSize;
		}
	}

	// Set draw type
	FString DrawAsStr;
	if (UAgentFrameworkActionUtils::TryGetStringParam(BrushParams, TEXT("draw_as"), DrawAsStr, Result.Errors, false))
	{
		if (DrawAsStr.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
			NewBrush.DrawAs = ESlateBrushDrawType::Box;
		else if (DrawAsStr.Equals(TEXT("Border"), ESearchCase::IgnoreCase))
			NewBrush.DrawAs = ESlateBrushDrawType::Border;
		else if (DrawAsStr.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
			NewBrush.DrawAs = ESlateBrushDrawType::Image;
		else if (DrawAsStr.Equals(TEXT("NoDrawType"), ESearchCase::IgnoreCase))
			NewBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	}

	// Set 9-slice margin
	FString MarginStr;
	if (UAgentFrameworkActionUtils::TryGetStringParam(BrushParams, TEXT("margin"), MarginStr, Result.Errors, false))
	{
		FMargin BrushMargin;
		if (ParseMargin(MarginStr, BrushMargin))
		{
			NewBrush.Margin = BrushMargin;
		}
	}

	// Apply brush to the correct target
	bool bApplied = false;

	// === Image widget ===
	UImage* ImageWidget = Cast<UImage>(Widget);
	if (IsValid(ImageWidget) && BrushTarget.Equals(TEXT("Brush"), ESearchCase::IgnoreCase))
	{
		ImageWidget->SetBrush(NewBrush);
		bApplied = true;
	}

	// === Button widget ===
	UButton* ButtonWidget = Cast<UButton>(Widget);
	if (IsValid(ButtonWidget))
	{
		FButtonStyle ButtonStyle = ButtonWidget->GetStyle();
		
		if (BrushTarget.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
		{
			ButtonStyle.Normal = NewBrush;
			bApplied = true;
		}
		else if (BrushTarget.Equals(TEXT("Hovered"), ESearchCase::IgnoreCase))
		{
			ButtonStyle.Hovered = NewBrush;
			bApplied = true;
		}
		else if (BrushTarget.Equals(TEXT("Pressed"), ESearchCase::IgnoreCase))
		{
			ButtonStyle.Pressed = NewBrush;
			bApplied = true;
		}
		else if (BrushTarget.Equals(TEXT("Disabled"), ESearchCase::IgnoreCase))
		{
			ButtonStyle.Disabled = NewBrush;
			bApplied = true;
		}
		
		if (bApplied)
		{
			ButtonWidget->SetStyle(ButtonStyle);
		}
	}

	// === Border widget ===
	UBorder* BorderWidget = Cast<UBorder>(Widget);
	if (IsValid(BorderWidget) && BrushTarget.Equals(TEXT("Background"), ESearchCase::IgnoreCase))
	{
		BorderWidget->SetBrush(NewBrush);
		bApplied = true;
	}

	// === ProgressBar widget ===
	UProgressBar* PBWidget = Cast<UProgressBar>(Widget);
	if (IsValid(PBWidget) && BrushTarget.Equals(TEXT("FillImage"), ESearchCase::IgnoreCase))
	{
		FProgressBarStyle PBStyle = PBWidget->GetWidgetStyle();
		PBStyle.FillImage = NewBrush;
		PBWidget->SetWidgetStyle(PBStyle);
		bApplied = true;
	}

	if (!bApplied)
	{
		// Try generic reflection as fallback
		FProperty* BrushProp = Widget->GetClass()->FindPropertyByName(FName(*BrushTarget));
		if (BrushProp)
		{
			FSlateBrush* BrushPtr = BrushProp->ContainerPtrToValuePtr<FSlateBrush>(Widget);
			if (BrushPtr)
			{
				*BrushPtr = NewBrush;
				bApplied = true;
			}
		}

		if (!bApplied)
		{
			Result.Errors.Add(FString::Printf(TEXT("Could not apply brush to '%s.%s'. Widget type '%s' doesn't support brush_target '%s'. For Image use 'Brush', for Button use 'Normal'/'Hovered'/'Pressed'/'Disabled', for Border use 'Background', for ProgressBar use 'FillImage'."), *WidgetName, *BrushTarget, *Widget->GetClass()->GetName(), *BrushTarget));
			return false;
		}
	}

	return true;
}

bool FAgentFrameworkWidgetActions::ApplyWidgetEventHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const FString& EventName, const FString& FunctionName, FAgentFrameworkActionResult& Result)
{
	if (!IsValid(WidgetBP) || !IsValid(Widget)) return false;

	FString WidgetName = Widget->GetName();

	// Find the delegate property on the widget class
	FMulticastDelegateProperty* DelegateProp = CastField<FMulticastDelegateProperty>(
		Widget->GetClass()->FindPropertyByName(FName(*EventName))
	);

	if (!DelegateProp)
	{
		// Build list of available events for this widget type
		FString AvailableEvents;
		for (TFieldIterator<FMulticastDelegateProperty> It(Widget->GetClass()); It; ++It)
		{
			if (!AvailableEvents.IsEmpty()) AvailableEvents += TEXT(", ");
			AvailableEvents += It->GetName();
		}
		Result.Errors.Add(FString::Printf(TEXT("Event '%s' not found on widget '%s' (%s). Available events: [%s]"), *EventName, *WidgetName, *Widget->GetClass()->GetName(), *AvailableEvents));
		return false;
	}

	// Get the EventGraph
	UEdGraph* EventGraph = nullptr;
	for (UEdGraph* Graph : WidgetBP->UbergraphPages)
	{
		if (IsValid(Graph) && Graph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
		{
			EventGraph = Graph;
			break;
		}
	}

	if (!IsValid(EventGraph) && WidgetBP->UbergraphPages.Num() > 0)
	{
		EventGraph = WidgetBP->UbergraphPages[0];
	}

	if (!IsValid(EventGraph))
	{
		Result.Errors.Add(TEXT("Widget Blueprint has no EventGraph. This is unexpected — try recompiling the Widget Blueprint first."));
		return false;
	}

	EventGraph->Modify();

	// Create a K2Node_ComponentBoundEvent
	UK2Node_ComponentBoundEvent* EventNode = NewObject<UK2Node_ComponentBoundEvent>(EventGraph);
	if (!IsValid(EventNode))
	{
		Result.Errors.Add(TEXT("Failed to create K2Node_ComponentBoundEvent node."));
		return false;
	}

	EventNode->DelegatePropertyName = DelegateProp->GetFName();
	EventNode->DelegateOwnerClass = Widget->GetClass();
	EventNode->ComponentPropertyName = FName(*WidgetName);

	// Position the node (find an empty area)
	int32 MaxY = 0;
	for (UEdGraphNode* Node : EventGraph->Nodes)
	{
		if (IsValid(Node))
		{
			MaxY = FMath::Max(MaxY, Node->NodePosY + 200);
		}
	}
	EventNode->NodePosX = 0;
	EventNode->NodePosY = MaxY + 100;

EventGraph->AddNode(EventNode, false, false);
	EventNode->CreateNewGuid();
	EventNode->PostPlacedNewNode();
	EventNode->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	return true;
}

bool FAgentFrameworkWidgetActions::ApplyWidgetSlotHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& SlotParams, FAgentFrameworkActionResult& Result)
{
	if (!IsValid(WidgetBP) || !IsValid(Widget) || !SlotParams.IsValid()) return false;

	FString WidgetName = Widget->GetName();

	if (!IsValid(Widget->Slot))
	{
		Result.Errors.Add(FString::Printf(TEXT("Widget '%s' has no slot. It may be the root widget (root widgets don't have slots) or not yet attached to a parent panel. Add it to a panel first via add_widget."), *WidgetName));
		return false;
	}

	Widget->Slot->Modify();

	TSharedPtr<FJsonObject> EffectiveParams = GetEffectiveSlotParams(SlotParams);
	FString AppliedSettings;

	// ====== CanvasPanelSlot ======
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (IsValid(CanvasSlot))
	{
		FAnchors Anchors;
		if (TryGetAnchors(EffectiveParams, Anchors))
		{
			CanvasSlot->SetAnchors(Anchors);
			AppliedSettings += FString::Printf(TEXT("anchors=(%.1f,%.1f)-(%.1f,%.1f) "), Anchors.Minimum.X, Anchors.Minimum.Y, Anchors.Maximum.X, Anchors.Maximum.Y);
		}

		FMargin Offsets;
		if (TryGetMargin(EffectiveParams, TEXT("offsets"), TEXT("Offsets"), Offsets))
		{
			CanvasSlot->SetOffsets(Offsets);
			AppliedSettings += FString::Printf(TEXT("offsets=(%.0f,%.0f,%.0f,%.0f) "), Offsets.Left, Offsets.Top, Offsets.Right, Offsets.Bottom);
		}

		FVector2D Alignment;
		if (TryGetAlignment(EffectiveParams, Alignment))
		{
			CanvasSlot->SetAlignment(Alignment);
			AppliedSettings += FString::Printf(TEXT("alignment=(%.1f,%.1f) "), Alignment.X, Alignment.Y);
		}

		bool bAutoSize = false;
		if (TryGetBoolValue(EffectiveParams, TEXT("auto_size"), TEXT("AutoSize"), TEXT("bAutoSize"), bAutoSize))
		{
			CanvasSlot->SetAutoSize(bAutoSize);
			AppliedSettings += FString::Printf(TEXT("auto_size=%s "), bAutoSize ? TEXT("true") : TEXT("false"));
		}

		int32 ZOrder = 0;
		if (TryGetIntValue(EffectiveParams, TEXT("z_order"), TEXT("ZOrder"), TEXT("zorder"), ZOrder))
		{
			CanvasSlot->SetZOrder(ZOrder);
			AppliedSettings += FString::Printf(TEXT("z_order=%d "), ZOrder);
		}
	}

	// ====== VerticalBoxSlot ======
	UVerticalBoxSlot* VBSlot = Cast<UVerticalBoxSlot>(Widget->Slot);
	if (IsValid(VBSlot))
	{
		FMargin Padding;
		if (TryGetMargin(EffectiveParams, TEXT("padding"), TEXT("Padding"), Padding))
		{
			VBSlot->SetPadding(Padding);
			AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
		}

		FString SizeRuleStr;
		ESlateSizeRule::Type SizeRule;
		if (TryGetSizeRule(EffectiveParams, SizeRuleStr, SizeRule))
		{
			VBSlot->SetSize(FSlateChildSize(SizeRule));
			AppliedSettings += FString::Printf(TEXT("size=%s "), SizeRule == ESlateSizeRule::Fill ? TEXT("Fill") : TEXT("Auto"));
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			VBSlot->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			VBSlot->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== HorizontalBoxSlot ======
	UHorizontalBoxSlot* HBSlot = Cast<UHorizontalBoxSlot>(Widget->Slot);
	if (IsValid(HBSlot))
	{
		FMargin Padding;
		if (TryGetMargin(EffectiveParams, TEXT("padding"), TEXT("Padding"), Padding))
		{
			HBSlot->SetPadding(Padding);
			AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
		}

		FString SizeRuleStr;
		ESlateSizeRule::Type SizeRule;
		if (TryGetSizeRule(EffectiveParams, SizeRuleStr, SizeRule))
		{
			HBSlot->SetSize(FSlateChildSize(SizeRule));
			AppliedSettings += FString::Printf(TEXT("size=%s "), SizeRule == ESlateSizeRule::Fill ? TEXT("Fill") : TEXT("Auto"));
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			HBSlot->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			HBSlot->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== OverlaySlot ======
	UOverlaySlot* OverlaySlotPtr = Cast<UOverlaySlot>(Widget->Slot);
	if (IsValid(OverlaySlotPtr))
	{
		FMargin Padding;
		if (TryGetMargin(EffectiveParams, TEXT("padding"), TEXT("Padding"), Padding))
		{
			OverlaySlotPtr->SetPadding(Padding);
			AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			OverlaySlotPtr->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			OverlaySlotPtr->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== GridSlot ======
	UGridSlot* GridSlotPtr = Cast<UGridSlot>(Widget->Slot);
	if (IsValid(GridSlotPtr))
	{
		int32 Row = 0;
		if (TryGetIntValue(EffectiveParams, TEXT("row"), TEXT("Row"), TEXT("grid_row"), Row))
		{
			GridSlotPtr->SetRow(Row);
			AppliedSettings += FString::Printf(TEXT("row=%d "), Row);
		}

		int32 Column = 0;
		if (TryGetIntValue(EffectiveParams, TEXT("column"), TEXT("Column"), TEXT("grid_column"), Column))
		{
			GridSlotPtr->SetColumn(Column);
			AppliedSettings += FString::Printf(TEXT("column=%d "), Column);
		}

		int32 RowSpan = 1;
		if (TryGetIntValue(EffectiveParams, TEXT("row_span"), TEXT("RowSpan"), TEXT("rowspan"), RowSpan))
		{
			GridSlotPtr->SetRowSpan(RowSpan);
			AppliedSettings += FString::Printf(TEXT("row_span=%d "), RowSpan);
		}

		int32 ColumnSpan = 1;
		if (TryGetIntValue(EffectiveParams, TEXT("column_span"), TEXT("ColumnSpan"), TEXT("columnspan"), ColumnSpan))
		{
			GridSlotPtr->SetColumnSpan(ColumnSpan);
			AppliedSettings += FString::Printf(TEXT("column_span=%d "), ColumnSpan);
		}

		int32 Layer = 0;
		if (TryGetIntValue(EffectiveParams, TEXT("layer"), TEXT("Layer"), TEXT(""), Layer))
		{
			GridSlotPtr->SetLayer(Layer);
			AppliedSettings += FString::Printf(TEXT("layer=%d "), Layer);
		}

		FVector2D Nudge;
		const TSharedPtr<FJsonObject>* NudgeObj = nullptr;
		FString NudgeStr;
		if (EffectiveParams->TryGetObjectField(TEXT("nudge"), NudgeObj) || EffectiveParams->TryGetObjectField(TEXT("Nudge"), NudgeObj))
		{
			if (NudgeObj && NudgeObj->IsValid())
			{
				double X = 0.0, Y = 0.0;
				(*NudgeObj)->TryGetNumberField(TEXT("x"), X);
				(*NudgeObj)->TryGetNumberField(TEXT("y"), Y);
				GridSlotPtr->SetNudge(FVector2D(X, Y));
				AppliedSettings += FString::Printf(TEXT("nudge=(%.1f,%.1f) "), X, Y);
			}
		}
		else if (EffectiveParams->TryGetStringField(TEXT("nudge"), NudgeStr) || EffectiveParams->TryGetStringField(TEXT("Nudge"), NudgeStr))
		{
			if (ParseVector2D(NudgeStr, Nudge))
			{
				GridSlotPtr->SetNudge(Nudge);
				AppliedSettings += FString::Printf(TEXT("nudge=(%.1f,%.1f) "), Nudge.X, Nudge.Y);
			}
		}

		FMargin Padding;
		if (TryGetMargin(EffectiveParams, TEXT("padding"), TEXT("Padding"), Padding))
		{
			GridSlotPtr->SetPadding(Padding);
			AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			GridSlotPtr->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			GridSlotPtr->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== UniformGridSlot ======
	UUniformGridSlot* UniformSlot = Cast<UUniformGridSlot>(Widget->Slot);
	if (IsValid(UniformSlot))
	{
		int32 Row = 0;
		if (TryGetIntValue(EffectiveParams, TEXT("row"), TEXT("Row"), TEXT("grid_row"), Row))
		{
			UniformSlot->SetRow(Row);
			AppliedSettings += FString::Printf(TEXT("row=%d "), Row);
		}

		int32 Column = 0;
		if (TryGetIntValue(EffectiveParams, TEXT("column"), TEXT("Column"), TEXT("grid_column"), Column))
		{
			UniformSlot->SetColumn(Column);
			AppliedSettings += FString::Printf(TEXT("column=%d "), Column);
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			UniformSlot->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			UniformSlot->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== ScrollBoxSlot ======
	UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(Widget->Slot);
	if (IsValid(ScrollSlot))
	{
		FMargin Padding;
		if (TryGetMargin(EffectiveParams, TEXT("padding"), TEXT("Padding"), Padding))
		{
			ScrollSlot->SetPadding(Padding);
			AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			ScrollSlot->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			ScrollSlot->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== WrapBoxSlot ======
	UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(Widget->Slot);
	if (IsValid(WrapSlot))
	{
		FMargin Padding;
		if (TryGetMargin(EffectiveParams, TEXT("padding"), TEXT("Padding"), Padding))
		{
			WrapSlot->SetPadding(Padding);
			AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			WrapSlot->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			WrapSlot->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}

		bool bFillEmptySpace = false;
		if (TryGetBoolValue(EffectiveParams, TEXT("fill_empty_space"), TEXT("FillEmptySpace"), TEXT("fill"), bFillEmptySpace))
		{
			WrapSlot->SetFillEmptySpace(bFillEmptySpace);
			AppliedSettings += FString::Printf(TEXT("fill=%s "), bFillEmptySpace ? TEXT("true") : TEXT("false"));
		}

		float FillSpan = 0.0f;
		if (TryGetFloatValue(EffectiveParams, TEXT("fill_span_when_less_than"), TEXT("FillSpanWhenLessThan"), FillSpan))
		{
			WrapSlot->SetFillSpanWhenLessThan(FillSpan);
			AppliedSettings += FString::Printf(TEXT("fill_span_when_less_than=%.1f "), FillSpan);
		}
	}

	// ====== WidgetSwitcherSlot ======
	UWidgetSwitcherSlot* SwitcherSlot = Cast<UWidgetSwitcherSlot>(Widget->Slot);
	if (IsValid(SwitcherSlot))
	{
		FMargin Padding;
		if (TryGetMargin(EffectiveParams, TEXT("padding"), TEXT("Padding"), Padding))
		{
			SwitcherSlot->SetPadding(Padding);
			AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			SwitcherSlot->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			SwitcherSlot->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== ScaleBoxSlot ======
	UScaleBoxSlot* ScaleSlot = Cast<UScaleBoxSlot>(Widget->Slot);
	if (IsValid(ScaleSlot))
	{
		FMargin Padding;
		if (TryGetMargin(EffectiveParams, TEXT("padding"), TEXT("Padding"), Padding))
		{
			ScaleSlot->SetPadding(Padding);
			AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			ScaleSlot->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			ScaleSlot->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== BorderSlot ======
	UBorderSlot* BorderSlotPtr = Cast<UBorderSlot>(Widget->Slot);
	if (IsValid(BorderSlotPtr))
	{
		FMargin Padding;
		if (TryGetMargin(EffectiveParams, TEXT("padding"), TEXT("Padding"), Padding))
		{
			BorderSlotPtr->SetPadding(Padding);
			AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			BorderSlotPtr->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			BorderSlotPtr->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== SizeBoxSlot ======
	USizeBoxSlot* SizeSlot = Cast<USizeBoxSlot>(Widget->Slot);
	if (IsValid(SizeSlot))
	{
		FMargin Padding;
		if (TryGetMargin(EffectiveParams, TEXT("padding"), TEXT("Padding"), Padding))
		{
			SizeSlot->SetPadding(Padding);
			AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
		}

		FString HAlignStr;
		EHorizontalAlignment HAlign;
		if (TryGetHAlign(EffectiveParams, HAlignStr, HAlign))
		{
			SizeSlot->SetHorizontalAlignment(HAlign);
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		FString VAlignStr;
		EVerticalAlignment VAlign;
		if (TryGetVAlign(EffectiveParams, VAlignStr, VAlign))
		{
			SizeSlot->SetVerticalAlignment(VAlign);
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	if (AppliedSettings.IsEmpty())
	{
		FString SlotType = IsValid(Widget->Slot) ? Widget->Slot->GetClass()->GetName() : TEXT("unknown");
		Result.Errors.Add(FString::Printf(TEXT("No slot properties were applied. Widget '%s' has slot type '%s'. Check that you're providing valid property names for this slot type."), *WidgetName, *SlotType));
		return false;
	}

	Result.ResultMessage = FString::Printf(TEXT("Configured slot on '%s': %s"), *WidgetName, *AppliedSettings.TrimEnd());
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteInstantiateUIHierarchy(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	if (!IsValid(WidgetBP->WidgetTree))
	{
		Result.Errors.Add(TEXT("Widget Blueprint has no WidgetTree — recreate the asset."));
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* WidgetsArray = nullptr;
	if (!UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("widgets"), WidgetsArray, Result.Errors, true))
	{
		return Result;
	}

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();

	int32 AddedCount = 0;
	int32 ConfiguredProperties = 0;
	int32 ConfiguredSlots = 0;
	int32 ConfiguredFonts = 0;
	int32 ConfiguredBrushes = 0;
	int32 ConfiguredEvents = 0;

	for (const TSharedPtr<FJsonValue>& WidgetVal : *WidgetsArray)
	{
		TSharedPtr<FJsonObject> WidgetObj = WidgetVal->AsObject();
		if (!WidgetObj.IsValid())
		{
			Result.Warnings.Add(TEXT("Skipping non-object item in widgets array."));
			continue;
		}

		FString WidgetClassName, WidgetName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(WidgetObj, TEXT("widget_class"), WidgetClassName, Result.Errors, false) ||
			!UAgentFrameworkActionUtils::TryGetStringParam(WidgetObj, TEXT("widget_name"), WidgetName, Result.Errors, false))
		{
			Result.Warnings.Add(TEXT("Skipping widget: missing widget_class or widget_name."));
			continue;
		}

		// Resolve class
		UClass* WidgetClass = ResolveWidgetClass(WidgetClassName);
		if (!IsValid(WidgetClass))
		{
			Result.Errors.Add(FString::Printf(TEXT("Widget class not found: '%s' for widget '%s'."), *WidgetClassName, *WidgetName));
			return Result;
		}

		// Construct widget
		UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
		if (!IsValid(NewWidget))
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to construct widget '%s' of class '%s'."), *WidgetName, *WidgetClassName));
			return Result;
		}

		WidgetBP->WidgetVariableNameToGuidMap.Add(NewWidget->GetFName(), FGuid::NewGuid());
		AddedCount++;

		// Attach to parent panel if specified
		FString ParentWidgetName;
		bool bAddedToParent = false;
		if (UAgentFrameworkActionUtils::TryGetStringParam(WidgetObj, TEXT("parent_widget"), ParentWidgetName, Result.Errors, false) && !ParentWidgetName.IsEmpty())
		{
			UWidget* ParentWidgetRaw = WidgetBP->WidgetTree->FindWidget(FName(*ParentWidgetName));
			UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidgetRaw);
			if (IsValid(ParentPanel))
			{
				ParentPanel->AddChild(NewWidget);
				bAddedToParent = true;
			}
			else if (IsValid(ParentWidgetRaw))
			{
				UContentWidget* ContentParent = Cast<UContentWidget>(ParentWidgetRaw);
				if (IsValid(ContentParent))
				{
					if (ContentParent->GetChildrenCount() > 0)
					{
						ContentParent->ClearChildren();
					}
					ContentParent->AddChild(NewWidget);
					bAddedToParent = true;
				}
			}
		}

		// If not added to parent and no root exists, set as root
		if (!bAddedToParent && !IsValid(WidgetBP->WidgetTree->RootWidget))
		{
			WidgetBP->WidgetTree->RootWidget = NewWidget;
		}
		else if (!bAddedToParent)
		{
			// Try attaching to root panel if root is a panel
			UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
			if (IsValid(RootPanel))
			{
				RootPanel->AddChild(NewWidget);
			}
		}

		// Apply properties
		const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(WidgetObj, TEXT("properties"), PropertiesObj, Result.Errors, false) && PropertiesObj && PropertiesObj->IsValid())
		{
			for (auto PropIt = (*PropertiesObj)->Values.CreateConstIterator(); PropIt; ++PropIt)
			{
				FString PropName = FString(*PropIt.Key());
				FString PropVal;
				if (PropIt.Value()->TryGetString(PropVal))
				{
					if (ApplyWidgetPropertyHelper(WidgetBP, NewWidget, PropName, PropVal, Result))
					{
						ConfiguredProperties++;
					}
				}
			}
		}

		// Apply slot settings
		const TSharedPtr<FJsonObject>* SlotObj = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(WidgetObj, TEXT("slot"), SlotObj, Result.Errors, false) && SlotObj && SlotObj->IsValid())
		{
			if (ApplyWidgetSlotHelper(WidgetBP, NewWidget, *SlotObj, Result))
			{
				ConfiguredSlots++;
			}
		}

		// Apply font settings
		const TSharedPtr<FJsonObject>* FontObj = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(WidgetObj, TEXT("font"), FontObj, Result.Errors, false) && FontObj && FontObj->IsValid())
		{
			if (ApplyWidgetFontHelper(WidgetBP, NewWidget, *FontObj, Result))
			{
				ConfiguredFonts++;
			}
		}

		// Apply brush settings
		const TSharedPtr<FJsonObject>* BrushObj = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(WidgetObj, TEXT("brush"), BrushObj, Result.Errors, false) && BrushObj && BrushObj->IsValid())
		{
			if (ApplyWidgetBrushHelper(WidgetBP, NewWidget, *BrushObj, Result))
			{
				ConfiguredBrushes++;
			}
		}

		// Apply event bindings
		const TSharedPtr<FJsonObject>* EventsObj = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(WidgetObj, TEXT("events"), EventsObj, Result.Errors, false) && EventsObj && EventsObj->IsValid())
		{
			for (auto EventIt = (*EventsObj)->Values.CreateConstIterator(); EventIt; ++EventIt)
			{
				FString EventName = FString(*EventIt.Key());
				FString FunctionName;
				EventIt.Value()->TryGetString(FunctionName);
				if (ApplyWidgetEventHelper(WidgetBP, NewWidget, EventName, FunctionName, Result))
				{
					ConfiguredEvents++;
				}
			}
		}
	}

	CompileAndMarkDirty(WidgetBP);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(
		TEXT("Successfully instantiated UI hierarchy in '%s': added %d widgets, configured %d properties, %d slots, %d fonts, %d brushes, %d event bindings."),
		*AssetPath, AddedCount, ConfiguredProperties, ConfiguredSlots, ConfiguredFonts, ConfiguredBrushes, ConfiguredEvents);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// Phase B Missing Hooks
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteGetWidgetInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath, WidgetName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!IsValid(Widget)) return Result;

	TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
	InfoObj->SetStringField(TEXT("name"), Widget->GetName());
	InfoObj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
	InfoObj->SetBoolField(TEXT("is_root"), IsValid(WidgetBP->WidgetTree) && Widget == WidgetBP->WidgetTree->RootWidget);

	UPanelWidget* ParentPanel = Widget->GetParent();
	if (IsValid(ParentPanel))
	{
		InfoObj->SetStringField(TEXT("parent"), ParentPanel->GetName());
	}
	else
	{
		InfoObj->SetStringField(TEXT("parent"), TEXT("(none)"));
	}

	if (IsValid(Widget->Slot))
	{
		InfoObj->SetStringField(TEXT("slot_type"), Widget->Slot->GetClass()->GetName());

		TSharedPtr<FJsonObject> SlotPropsObj = MakeShared<FJsonObject>();
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			FAnchors Anchors = CanvasSlot->GetAnchors();
			SlotPropsObj->SetStringField(TEXT("anchors_min"), FString::Printf(TEXT("%.2f,%.2f"), Anchors.Minimum.X, Anchors.Minimum.Y));
			SlotPropsObj->SetStringField(TEXT("anchors_max"), FString::Printf(TEXT("%.2f,%.2f"), Anchors.Maximum.X, Anchors.Maximum.Y));
			FMargin Offsets = CanvasSlot->GetOffsets();
			SlotPropsObj->SetStringField(TEXT("offsets"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f"), Offsets.Left, Offsets.Top, Offsets.Right, Offsets.Bottom));
			FVector2D Alignment = CanvasSlot->GetAlignment();
			SlotPropsObj->SetStringField(TEXT("alignment"), FString::Printf(TEXT("%.2f,%.2f"), Alignment.X, Alignment.Y));
			SlotPropsObj->SetBoolField(TEXT("auto_size"), CanvasSlot->GetAutoSize());
			SlotPropsObj->SetNumberField(TEXT("z_order"), CanvasSlot->GetZOrder());
		}
		else if (UVerticalBoxSlot* VBSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
		{
			FMargin Padding = VBSlot->GetPadding();
			SlotPropsObj->SetStringField(TEXT("padding"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f"), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom));
			SlotPropsObj->SetStringField(TEXT("size_rule"), VBSlot->GetSize().SizeRule == ESlateSizeRule::Fill ? TEXT("Fill") : TEXT("Auto"));
		}
		else if (UHorizontalBoxSlot* HBSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
		{
			FMargin Padding = HBSlot->GetPadding();
			SlotPropsObj->SetStringField(TEXT("padding"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f"), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom));
			SlotPropsObj->SetStringField(TEXT("size_rule"), HBSlot->GetSize().SizeRule == ESlateSizeRule::Fill ? TEXT("Fill") : TEXT("Auto"));
		}
		else if (UGridSlot* GridSlot = Cast<UGridSlot>(Widget->Slot))
		{
			SlotPropsObj->SetNumberField(TEXT("row"), GridSlot->GetRow());
			SlotPropsObj->SetNumberField(TEXT("column"), GridSlot->GetColumn());
			SlotPropsObj->SetNumberField(TEXT("row_span"), GridSlot->GetRowSpan());
			SlotPropsObj->SetNumberField(TEXT("column_span"), GridSlot->GetColumnSpan());
		}
		InfoObj->SetObjectField(TEXT("slot_properties"), SlotPropsObj);
	}

	UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
	if (IsValid(Panel))
	{
		InfoObj->SetBoolField(TEXT("is_panel"), true);
		InfoObj->SetNumberField(TEXT("child_count"), Panel->GetChildrenCount());

		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			UWidget* Child = Panel->GetChildAt(Index);
			if (IsValid(Child))
			{
				ChildrenArray.Add(MakeShared<FJsonValueString>(Child->GetName()));
			}
		}
		InfoObj->SetArrayField(TEXT("children"), ChildrenArray);
	}
	else
	{
		InfoObj->SetBoolField(TEXT("is_panel"), false);
	}

	// Key properties summary
	TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
	PropsObj->SetBoolField(TEXT("bIsEnabled"), Widget->GetIsEnabled());
	PropsObj->SetNumberField(TEXT("RenderOpacity"), Widget->GetRenderOpacity());

	if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
	{
		PropsObj->SetStringField(TEXT("Text"), TextBlock->GetText().ToString());
	}
	InfoObj->SetObjectField(TEXT("properties"), PropsObj);

	FString OutputStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputStr);
	FJsonSerializer::Serialize(InfoObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = OutputStr;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteClearPanelChildren(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath, WidgetName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("widget_name"), WidgetName, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!IsValid(Widget)) return Result;

	UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
	UContentWidget* Content = Cast<UContentWidget>(Widget);

	int32 RemovedCount = 0;
	if (IsValid(Panel))
	{
		RemovedCount = Panel->GetChildrenCount();
		WidgetBP->Modify();
		Panel->Modify();
		Panel->ClearChildren();
	}
	else if (IsValid(Content))
	{
		RemovedCount = Content->GetChildrenCount();
		WidgetBP->Modify();
		Content->Modify();
		Content->ClearChildren();
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Widget '%s' (%s) is neither a UPanelWidget nor a UContentWidget and cannot contain children."), *WidgetName, *Widget->GetClass()->GetName()));
		return Result;
	}

	CompileAndMarkDirty(WidgetBP);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Cleared %d children from panel widget '%s' in '%s'."), RemovedCount, *WidgetName, *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkWidgetActions::ExecuteGetWidgetSlots(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!IsValid(WidgetBP)) return Result;

	TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetStringField(TEXT("asset_path"), WidgetBP->GetPathName());

	TArray<TSharedPtr<FJsonValue>> SlotsArray;
	if (IsValid(WidgetBP->WidgetTree))
	{
		WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (!IsValid(Widget) || !IsValid(Widget->Slot)) return;

			TSharedPtr<FJsonObject> SlotItem = MakeShared<FJsonObject>();
			SlotItem->SetStringField(TEXT("widget_name"), Widget->GetName());
			SlotItem->SetStringField(TEXT("widget_class"), Widget->GetClass()->GetName());

			UPanelWidget* Parent = Widget->GetParent();
			if (IsValid(Parent))
			{
				SlotItem->SetStringField(TEXT("parent_name"), Parent->GetName());
			}

			SlotItem->SetStringField(TEXT("slot_type"), Widget->Slot->GetClass()->GetName());

			TSharedPtr<FJsonObject> SlotPropsObj = MakeShared<FJsonObject>();
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
			{
				FAnchors Anchors = CanvasSlot->GetAnchors();
				SlotPropsObj->SetStringField(TEXT("anchors_min"), FString::Printf(TEXT("%.2f,%.2f"), Anchors.Minimum.X, Anchors.Minimum.Y));
				SlotPropsObj->SetStringField(TEXT("anchors_max"), FString::Printf(TEXT("%.2f,%.2f"), Anchors.Maximum.X, Anchors.Maximum.Y));
				FMargin Offsets = CanvasSlot->GetOffsets();
				SlotPropsObj->SetStringField(TEXT("offsets"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f"), Offsets.Left, Offsets.Top, Offsets.Right, Offsets.Bottom));
				FVector2D Alignment = CanvasSlot->GetAlignment();
				SlotPropsObj->SetStringField(TEXT("alignment"), FString::Printf(TEXT("%.2f,%.2f"), Alignment.X, Alignment.Y));
				SlotPropsObj->SetBoolField(TEXT("auto_size"), CanvasSlot->GetAutoSize());
				SlotPropsObj->SetNumberField(TEXT("z_order"), CanvasSlot->GetZOrder());
			}
			else if (UVerticalBoxSlot* VBSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
			{
				FMargin Padding = VBSlot->GetPadding();
				SlotPropsObj->SetStringField(TEXT("padding"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f"), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom));
				SlotPropsObj->SetStringField(TEXT("size_rule"), VBSlot->GetSize().SizeRule == ESlateSizeRule::Fill ? TEXT("Fill") : TEXT("Auto"));
			}
			else if (UHorizontalBoxSlot* HBSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
			{
				FMargin Padding = HBSlot->GetPadding();
				SlotPropsObj->SetStringField(TEXT("padding"), FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f"), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom));
				SlotPropsObj->SetStringField(TEXT("size_rule"), HBSlot->GetSize().SizeRule == ESlateSizeRule::Fill ? TEXT("Fill") : TEXT("Auto"));
			}
			else if (UGridSlot* GridSlot = Cast<UGridSlot>(Widget->Slot))
			{
				SlotPropsObj->SetNumberField(TEXT("row"), GridSlot->GetRow());
				SlotPropsObj->SetNumberField(TEXT("column"), GridSlot->GetColumn());
				SlotPropsObj->SetNumberField(TEXT("row_span"), GridSlot->GetRowSpan());
				SlotPropsObj->SetNumberField(TEXT("column_span"), GridSlot->GetColumnSpan());
			}

			SlotItem->SetObjectField(TEXT("slot_properties"), SlotPropsObj);
			SlotsArray.Add(MakeShared<FJsonValueObject>(SlotItem));
		});
	}

	RootObj->SetArrayField(TEXT("slots"), SlotsArray);
	RootObj->SetNumberField(TEXT("total_slotted_widgets"), SlotsArray.Num());

	FString OutputStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputStr);
	FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = OutputStr;
	return Result;
}
