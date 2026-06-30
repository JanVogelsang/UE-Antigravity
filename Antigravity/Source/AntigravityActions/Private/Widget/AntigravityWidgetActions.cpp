// Copyright 2026 Antigravity. All Rights Reserved.

#include "Widget/AntigravityWidgetActions.h"
#include "AntigravityCoreModule.h"

// UMG Runtime â€” Panels
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
#include "Components/Border.h"
#include "Components/BackgroundBlur.h"
#include "Components/InvalidationBox.h"
#include "Components/RetainerBox.h"
#include "Components/NamedSlot.h"

// UMG Runtime â€” Leaf Widgets
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
#include "Viewport/AntigravityViewportActions.h"
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
	FString ExpandAssetPath(const FString& InPath)
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

	FString CompressAssetPath(const FString& InPath)
	{
		if (InPath.StartsWith(TEXT("/Game/")))
		{
			return InPath.Mid(6);
		}
		return InPath;
	}
}

FAntigravityWidgetActions::FAntigravityWidgetActions() {}
FAntigravityWidgetActions::~FAntigravityWidgetActions() {}

FName FAntigravityWidgetActions::GetActionName() const { return FName(TEXT("Widget")); }
FText FAntigravityWidgetActions::GetDisplayName() const { return FText::FromString(TEXT("Widget Actions")); }
EAntigravityActionCategory FAntigravityWidgetActions::GetCategory() const { return EAntigravityActionCategory::Blueprint; }
EAntigravityRiskLevel FAntigravityWidgetActions::GetDefaultRiskLevel() const { return EAntigravityRiskLevel::Medium; }
bool FAntigravityWidgetActions::CanUndo() const { return true; }
bool FAntigravityWidgetActions::UndoAction() { return false; }

TArray<FString> FAntigravityWidgetActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_widget_blueprint"),
		TEXT("add_widget"),
		TEXT("set_widget_slot"),
		TEXT("set_widget_property"),
		TEXT("set_widget_font"),
		TEXT("set_widget_brush"),
		TEXT("bind_widget_event"),
		TEXT("remove_widget"),
		TEXT("get_widget_tree"),
		TEXT("compile_widget_blueprint"),
		TEXT("macro_create_basic_ui_menu"),
		TEXT("capture_widget"),
		TEXT("instantiate_ui_hierarchy")
	};
}

bool FAntigravityWidgetActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString RawAssetPath;
	if (Params->TryGetStringField(TEXT("asset_path"), RawAssetPath))
	{
		Params->SetStringField(TEXT("asset_path"), ExpandAssetPath(RawAssetPath));
	}

	if (!Params->HasField(TEXT("asset_path")))
	{
		OutErrors.Add(TEXT("Missing required field: asset_path"));
		return false;
	}

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		OutErrors.Add(TEXT("asset_path cannot be empty. Provide a valid content path starting with /Game/, e.g. /Game/UI/WBP_MainMenu"));
		return false;
	}

	if (!AssetPath.StartsWith(TEXT("/Game/")))
	{
		OutErrors.Add(FString::Printf(TEXT("asset_path '%s' must start with /Game/. Example: /Game/UI/WBP_MainMenu"), *AssetPath));
		return false;
	}

	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);

	if (ToolName == TEXT("add_widget"))
	{
		if (!Params->HasField(TEXT("widget_class")) || !Params->HasField(TEXT("widget_name")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for add_widget: widget_class, widget_name"));
			return false;
		}
	}
	else if (ToolName == TEXT("set_widget_slot"))
	{
		if (!Params->HasField(TEXT("widget_name")) || !Params->HasField(TEXT("slot_properties")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for set_widget_slot: widget_name, slot_properties"));
			return false;
		}
	}
	else if (ToolName == TEXT("set_widget_property"))
	{
		if (!Params->HasField(TEXT("widget_name")) || !Params->HasField(TEXT("property_name")) || !Params->HasField(TEXT("property_value")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for set_widget_property: widget_name, property_name, property_value"));
			return false;
		}
	}
	else if (ToolName == TEXT("set_widget_font"))
	{
		if (!Params->HasField(TEXT("widget_name")) || !Params->HasField(TEXT("font_path")) || !Params->HasField(TEXT("size")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for set_widget_font: widget_name, font_path, size"));
			return false;
		}
	}
	else if (ToolName == TEXT("set_widget_brush"))
	{
		if (!Params->HasField(TEXT("widget_name")) || !Params->HasField(TEXT("texture_path")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for set_widget_brush: widget_name, texture_path"));
			return false;
		}
	}
	else if (ToolName == TEXT("bind_widget_event"))
	{
		if (!Params->HasField(TEXT("widget_name")) || !Params->HasField(TEXT("event_name")) || !Params->HasField(TEXT("function_name")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for bind_widget_event: widget_name, event_name, function_name"));
			return false;
		}
	}
	else if (ToolName == TEXT("remove_widget"))
	{
		if (!Params->HasField(TEXT("widget_name")))
		{
			OutErrors.Add(TEXT("Missing required field for remove_widget: widget_name"));
			return false;
		}
	}
	else if (ToolName == TEXT("macro_create_basic_ui_menu"))
	{
		if (!Params->HasField(TEXT("menu_title")) || !Params->HasField(TEXT("button_names")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for macro_create_basic_ui_menu: menu_title, button_names"));
			return false;
		}
	}
	else if (ToolName == TEXT("instantiate_ui_hierarchy"))
	{
		if (!Params->HasField(TEXT("widgets")))
		{
			OutErrors.Add(TEXT("Missing required field for instantiate_ui_hierarchy: widgets (must be a JSON array)"));
			return false;
		}
	}

	return true;
}

// ============================================================================
// PreviewAction
// ============================================================================

FAntigravityActionPlan FAntigravityWidgetActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionPlan Plan;
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	Plan.Summary = FString::Printf(TEXT("Widget Blueprint operation at %s"), *AssetPath);

	FAntigravityAction Action;
	Action.Description = Plan.Summary;
	Action.Category = EAntigravityActionCategory::Blueprint;
	Action.RiskLevel = EAntigravityRiskLevel::Medium;
	Action.AffectedAssets.Add(AssetPath);
	Plan.Actions.Add(Action);
	Plan.MaxRiskLevel = EAntigravityRiskLevel::Medium;

	return Plan;
}

// ============================================================================
// ExecuteAction — Dispatch
// ============================================================================

FAntigravityActionResult FAntigravityWidgetActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);

	bool bIsReadOnly = (ToolName == TEXT("get_widget_tree") || ToolName == TEXT("capture_widget"));

	TOptional<FScopedTransaction> Transaction;
	if (!bIsReadOnly)
	{
		Transaction.Emplace(FText::FromString(TEXT("Antigravity Widget Action")));
	}

	FAntigravityActionResult Result;
	Result.bSuccess = false;

	if (ToolName == TEXT("create_widget_blueprint"))       Result = ExecuteCreateWidgetBlueprint(Params, Result);
	else if (ToolName == TEXT("add_widget"))               Result = ExecuteAddWidget(Params, Result);
	else if (ToolName == TEXT("set_widget_slot"))          Result = ExecuteSetWidgetSlot(Params, Result);
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
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown Widget tool: '%s'. Supported: create_widget_blueprint, add_widget, set_widget_slot, set_widget_property, set_widget_font, set_widget_brush, bind_widget_event, remove_widget, get_widget_tree, compile_widget_blueprint, macro_create_basic_ui_menu, capture_widget, instantiate_ui_hierarchy"), *ToolName));
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

UClass* FAntigravityWidgetActions::ResolveWidgetClass(const FString& ClassName)
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

	// Reflection search â€” try with and without U prefix
	UClass* Found = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
	if (!Found)
		Found = FindFirstObject<UClass>(*(TEXT("U") + ClassName), EFindFirstObjectOptions::None);
	return Found;
}

UWidgetBlueprint* FAntigravityWidgetActions::LoadWidgetBP(const FString& AssetPath, FAntigravityActionResult& Result)
{
	UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
	if (!WidgetBP)
	{
		Result.Errors.Add(FString::Printf(TEXT("Widget Blueprint not found: '%s'. Verify the asset exists and the path starts with /Game/."), *AssetPath));
	}
	return WidgetBP;
}

UWidget* FAntigravityWidgetActions::FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FAntigravityActionResult& Result)
{
	if (!WidgetBP->WidgetTree)
	{
		Result.Errors.Add(TEXT("Widget Blueprint has no WidgetTree â€” recreate the asset."));
		return nullptr;
	}

	UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
	if (!Widget)
	{
		// Build helpful list of existing widget names
		TArray<UWidget*> AllWidgets;
		WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
		FString AvailableNames;
		for (UWidget* W : AllWidgets)
		{
			if (!AvailableNames.IsEmpty()) AvailableNames += TEXT(", ");
			AvailableNames += W->GetName();
		}
		Result.Errors.Add(FString::Printf(TEXT("Widget '%s' not found. Available widgets: [%s]. Use get_widget_tree to see all widget names."), *WidgetName, *AvailableNames));
	}
	return Widget;
}

bool FAntigravityWidgetActions::ParseVector2D(const FString& Str, FVector2D& OutVec)
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

bool FAntigravityWidgetActions::ParseMargin(const FString& Str, FMargin& OutMargin)
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

EHorizontalAlignment FAntigravityWidgetActions::ParseHAlign(const FString& Str)
{
	if (Str.Equals(TEXT("Left"),   ESearchCase::IgnoreCase)) return HAlign_Left;
	if (Str.Equals(TEXT("Center"), ESearchCase::IgnoreCase)) return HAlign_Center;
	if (Str.Equals(TEXT("Right"),  ESearchCase::IgnoreCase)) return HAlign_Right;
	if (Str.Equals(TEXT("Fill"),   ESearchCase::IgnoreCase)) return HAlign_Fill;
	return HAlign_Fill; // default
}

EVerticalAlignment FAntigravityWidgetActions::ParseVAlign(const FString& Str)
{
	if (Str.Equals(TEXT("Top"),    ESearchCase::IgnoreCase)) return VAlign_Top;
	if (Str.Equals(TEXT("Center"), ESearchCase::IgnoreCase)) return VAlign_Center;
	if (Str.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase)) return VAlign_Bottom;
	if (Str.Equals(TEXT("Fill"),   ESearchCase::IgnoreCase)) return VAlign_Fill;
	return VAlign_Fill; // default
}

bool FAntigravityWidgetActions::ParseLinearColor(const FString& Str, FLinearColor& OutColor)
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

void FAntigravityWidgetActions::CompileAndMarkDirty(UWidgetBlueprint* WidgetBP)
{
	FKismetEditorUtilities::CompileBlueprint(WidgetBP, EBlueprintCompileOptions::SkipGarbageCollection);
	WidgetBP->GetOutermost()->MarkPackageDirty();
}

// ============================================================================
// BuildWidgetTreeJson
// ============================================================================

FString FAntigravityWidgetActions::BuildWidgetTreeJson(UWidgetBlueprint* WidgetBlueprint)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), WidgetBlueprint->GetPathName());
	Root->SetStringField(TEXT("parent_class"), WidgetBlueprint->ParentClass ? WidgetBlueprint->ParentClass->GetName() : TEXT("UserWidget"));

	TArray<TSharedPtr<FJsonValue>> WidgetsArray;

	if (WidgetBlueprint->WidgetTree)
	{
		WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (!Widget) return;
			TSharedPtr<FJsonObject> WObj = MakeShared<FJsonObject>();
			WObj->SetStringField(TEXT("name"), Widget->GetName());
			WObj->SetStringField(TEXT("class"), Widget->GetClass()->GetName());

			// Is this widget the root?
			WObj->SetBoolField(TEXT("is_root"), Widget == WidgetBlueprint->WidgetTree->RootWidget);

			// Parent name
			UPanelWidget* ParentPanel = Widget->GetParent();
			if (ParentPanel)
			{
				WObj->SetStringField(TEXT("parent"), ParentPanel->GetName());
			}
			else if (Widget != WidgetBlueprint->WidgetTree->RootWidget)
			{
				WObj->SetStringField(TEXT("parent"), TEXT("(orphaned)"));
			}

			// Is it a panel (can contain children)?
			UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
			if (Panel)
			{
				WObj->SetBoolField(TEXT("is_panel"), true);
				WObj->SetNumberField(TEXT("child_count"), Panel->GetChildrenCount());
			}
			else
			{
				WObj->SetBoolField(TEXT("is_panel"), false);
			}

			// Slot type
			if (Widget->Slot)
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

FAntigravityActionResult FAntigravityWidgetActions::ExecuteCreateWidgetBlueprint(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	// Validate asset_path early
	if (AssetPath.IsEmpty())
	{
		Result.Errors.Add(TEXT("asset_path is empty. You MUST provide a valid content path starting with /Game/, e.g. /Game/UI/WBP_MainMenu."));
		return Result;
	}
	if (!AssetPath.StartsWith(TEXT("/Game/")))
	{
		Result.Errors.Add(FString::Printf(TEXT("asset_path '%s' must start with /Game/. Example: /Game/UI/WBP_MainMenu"), *AssetPath));
		return Result;
	}

	FString RootWidgetClassName = TEXT("CanvasPanel");
	Params->TryGetStringField(TEXT("root_widget_class"), RootWidgetClassName);

	FString ParentClassName = TEXT("UserWidget");
	Params->TryGetStringField(TEXT("parent_class"), ParentClassName);

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName   = FPackageName::GetShortName(AssetPath);

	// Resolve parent class
	UClass* ParentClass = UUserWidget::StaticClass();
	if (!ParentClassName.IsEmpty() && ParentClassName != TEXT("UserWidget"))
	{
		UClass* FoundClass = FindFirstObject<UClass>(*ParentClassName, EFindFirstObjectOptions::None);
		if (!FoundClass)
			FoundClass = FindFirstObject<UClass>(*(TEXT("U") + ParentClassName), EFindFirstObjectOptions::None);
		if (FoundClass && FoundClass->IsChildOf(UUserWidget::StaticClass()))
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

	if (!NewWidget)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Widget Blueprint at '%s'. Verify the path is valid, starts with /Game/, and the directory exists. Example: /Game/UI/WBP_MainMenu"), *AssetPath));
		return Result;
	}

	// Set the root widget
	if (NewWidget->WidgetTree && !RootWidgetClassName.IsEmpty() && RootWidgetClassName != TEXT("none"))
	{
		UClass* RootClass = ResolveWidgetClass(RootWidgetClassName);
		if (RootClass)
		{
			NewWidget->Modify();
			NewWidget->WidgetTree->Modify();
			UWidget* RootWidget = NewWidget->WidgetTree->ConstructWidget<UWidget>(RootClass, FName(*RootWidgetClassName));
			if (RootWidget)
			{
				NewWidget->WidgetVariableNameToGuidMap.Add(RootWidget->GetFName(), FGuid::NewGuid());
				NewWidget->WidgetTree->RootWidget = RootWidget;
				UE_LOG(LogAntigravity, Log, TEXT("WidgetActions: Set root widget '%s' on '%s'"), *RootWidgetClassName, *AssetName);
			}
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Root widget class '%s' not found â€” Widget Blueprint created without a root widget. Valid classes: CanvasPanel, VerticalBox, HorizontalBox, SizeBox, Overlay, GridPanel, ScrollBox, WrapBox."), *RootWidgetClassName));
		}
	}

	// Compile
	FKismetEditorUtilities::CompileBlueprint(NewWidget, EBlueprintCompileOptions::SkipGarbageCollection);

	// Save
	UPackage* Package = NewWidget->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, NewWidget, *PackageFilename, SaveArgs);
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

FAntigravityActionResult FAntigravityWidgetActions::ExecuteMacroCreateBasicUIMenu(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString MenuTitle = Params->GetStringField(TEXT("menu_title"));

	TArray<FString> ButtonNames;
	if (Params->HasTypedField<EJson::Array>(TEXT("button_names")))
	{
		for (const TSharedPtr<FJsonValue>& Val : Params->GetArrayField(TEXT("button_names")))
		{
			ButtonNames.Add(Val->AsString());
		}
	}

	// 1. Create the base Widget Blueprint with a CanvasPanel root
	TSharedPtr<FJsonObject> CreateParams = MakeShared<FJsonObject>();
	CreateParams->SetStringField(TEXT("asset_path"), AssetPath);
	CreateParams->SetStringField(TEXT("root_widget_class"), TEXT("CanvasPanel"));

	FAntigravityActionResult CreateResult;
	ExecuteCreateWidgetBlueprint(CreateParams.ToSharedRef(), CreateResult);
	if (!CreateResult.bSuccess)
	{
		Result.Errors.Append(CreateResult.Errors);
		return Result;
	}

	// 2. Load the newly created asset
	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP || !WidgetBP->WidgetTree) return Result;

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBP->WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		Result.Errors.Add(TEXT("Root widget is not a CanvasPanel."));
		return Result;
	}

	// 3. Create Vertical Box and add to Canvas
	UVerticalBox* VBox = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuVerticalBox"));
	if (VBox)
	{
		WidgetBP->WidgetVariableNameToGuidMap.Add(VBox->GetFName(), FGuid::NewGuid());
	}
	UCanvasPanelSlot* VBoxSlot = Cast<UCanvasPanelSlot>(RootCanvas->AddChild(VBox));
	if (VBoxSlot)
	{
		FAnchors CenterAnchor(0.5f, 0.5f, 0.5f, 0.5f);
		VBoxSlot->SetAnchors(CenterAnchor);
		VBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		VBoxSlot->SetAutoSize(true);
	}

	// 4. Create Title Text
	UTextBlock* TitleText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	if (TitleText)
	{
		WidgetBP->WidgetVariableNameToGuidMap.Add(TitleText->GetFName(), FGuid::NewGuid());
		TitleText->SetText(FText::FromString(MenuTitle));
		UVerticalBoxSlot* TitleSlot = Cast<UVerticalBoxSlot>(VBox->AddChild(TitleText));
		if (TitleSlot)
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
		if (Btn)
		{
			WidgetBP->WidgetVariableNameToGuidMap.Add(Btn->GetFName(), FGuid::NewGuid());
			UVerticalBoxSlot* BtnSlot = Cast<UVerticalBoxSlot>(VBox->AddChild(Btn));
			if (BtnSlot)
			{
				BtnSlot->SetPadding(FMargin(0, 0, 0, 10));
				BtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}

			UTextBlock* BtnText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*(BtnWidgetName + TEXT("Text"))));
			if (BtnText)
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

	CompileAndMarkDirty(WidgetBP);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully created UI Menu '%s' with %d buttons."), *AssetPath, ButtonNames.Num());
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteAddWidget
// ============================================================================

FAntigravityActionResult FAntigravityWidgetActions::ExecuteAddWidget(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath      = Params->GetStringField(TEXT("asset_path"));
	FString WidgetClassName = Params->GetStringField(TEXT("widget_class"));
	FString WidgetName     = Params->GetStringField(TEXT("widget_name"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

	if (!WidgetBP->WidgetTree)
	{
		Result.Errors.Add(TEXT("Widget Blueprint has no WidgetTree â€” recreate the asset."));
		return Result;
	}

	UClass* WidgetClass = ResolveWidgetClass(WidgetClassName);
	if (!WidgetClass)
	{
		Result.Errors.Add(FString::Printf(TEXT("Widget class not found: '%s'. Valid classes: CanvasPanel, VerticalBox, HorizontalBox, TextBlock, Button, Image, ScrollBox, SizeBox, Overlay, WidgetSwitcher, ProgressBar, Slider, CheckBox, EditableTextBox, MultiLineEditableTextBox, ComboBoxString, SpinBox, Spacer, Border, BackgroundBlur, WrapBox, ScaleBox, RichTextBlock, Throbber, CircularThrobber, ExpandableArea, GridPanel, UniformGridPanel"), *WidgetClassName));
		return Result;
	}

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();

	UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
	if (!NewWidget)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to construct widget of class '%s'."), *WidgetClassName));
		return Result;
	}

	WidgetBP->WidgetVariableNameToGuidMap.Add(NewWidget->GetFName(), FGuid::NewGuid());

	// Attach to parent panel if specified
	FString ParentWidgetName;
	bool bAddedToParent = false;
	if (Params->TryGetStringField(TEXT("parent_widget"), ParentWidgetName) && !ParentWidgetName.IsEmpty())
	{
		UWidget* ParentWidgetRaw = WidgetBP->WidgetTree->FindWidget(FName(*ParentWidgetName));
		UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidgetRaw);
		if (ParentPanel)
		{
			UPanelSlot* Slot = ParentPanel->AddChild(NewWidget);
			bAddedToParent = true;

			// Report the slot type so the AI knows what properties are available
			if (Slot)
			{
				FString SlotType = Slot->GetClass()->GetName();
				Result.Warnings.Add(FString::Printf(TEXT("Widget added to '%s'. Slot type: %s. Use set_widget_slot to configure layout (anchors, padding, alignment)."), *ParentWidgetName, *SlotType));
			}
		}
		else if (ParentWidgetRaw)
		{
			// Check if it's a content widget (single child) â€” like Button, SizeBox, Border
			UContentWidget* ContentParent = Cast<UContentWidget>(ParentWidgetRaw);
			if (ContentParent)
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
	if (!bAddedToParent && !WidgetBP->WidgetTree->RootWidget)
	{
		WidgetBP->WidgetTree->RootWidget = NewWidget;
		Result.Warnings.Add(FString::Printf(TEXT("No parent_widget specified and no root exists â€” '%s' set as root widget."), *WidgetName));
	}
	else if (!bAddedToParent)
	{
		// Try to attach to root if it's a panel
		UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
		if (RootPanel)
		{
			RootPanel->AddChild(NewWidget);
			Result.Warnings.Add(FString::Printf(TEXT("No parent_widget specified â€” attached '%s' to root panel '%s' by default. Use set_widget_slot to configure layout."), *WidgetName, *RootPanel->GetName()));
		}
		else
		{
			Result.Warnings.Add(TEXT("Could not attach widget â€” specify parent_widget or set a panel as the root first."));
		}
	}

	CompileAndMarkDirty(WidgetBP);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Added widget '%s' (%s) to '%s'. IMPORTANT: If the parent is a CanvasPanel, you MUST call set_widget_slot to set anchors/position/size â€” otherwise the widget will be invisible (zero size at 0,0)."), *WidgetName, *WidgetClassName, *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteSetWidgetSlot
// ============================================================================

FAntigravityActionResult FAntigravityWidgetActions::ExecuteSetWidgetSlot(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath  = Params->GetStringField(TEXT("asset_path"));
	FString WidgetName = Params->GetStringField(TEXT("widget_name"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!Widget) return Result;

	if (!Widget->Slot)
	{
		Result.Errors.Add(FString::Printf(TEXT("Widget '%s' has no slot. It may be the root widget (root widgets don't have slots) or not yet attached to a parent panel. Add it to a panel first via add_widget."), *WidgetName));
		return Result;
	}

	WidgetBP->Modify();
	Widget->Slot->Modify();

	FString AppliedSettings;

	// ====== CanvasPanelSlot ======
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (CanvasSlot)
	{
		FString AnchorsMinStr, AnchorsMaxStr, OffsetsStr, AlignmentStr;
		bool bAutoSize = false;
		int32 ZOrder = 0;

		if (Params->TryGetStringField(TEXT("anchors_min"), AnchorsMinStr))
		{
			FVector2D AnchorMin;
			if (ParseVector2D(AnchorsMinStr, AnchorMin))
			{
				FAnchors Anchors = CanvasSlot->GetAnchors();
				Anchors.Minimum = AnchorMin;

				// If max is also provided, set both together
				if (Params->TryGetStringField(TEXT("anchors_max"), AnchorsMaxStr))
				{
					FVector2D AnchorMax;
					if (ParseVector2D(AnchorsMaxStr, AnchorMax))
					{
						Anchors.Maximum = AnchorMax;
					}
				}
				CanvasSlot->SetAnchors(Anchors);
				AppliedSettings += FString::Printf(TEXT("anchors=(%.1f,%.1f)-(%.1f,%.1f) "), Anchors.Minimum.X, Anchors.Minimum.Y, Anchors.Maximum.X, Anchors.Maximum.Y);
			}
		}
		else if (Params->TryGetStringField(TEXT("anchors_max"), AnchorsMaxStr))
		{
			FVector2D AnchorMax;
			if (ParseVector2D(AnchorsMaxStr, AnchorMax))
			{
				FAnchors Anchors = CanvasSlot->GetAnchors();
				Anchors.Maximum = AnchorMax;
				CanvasSlot->SetAnchors(Anchors);
				AppliedSettings += FString::Printf(TEXT("anchors_max=(%.1f,%.1f) "), AnchorMax.X, AnchorMax.Y);
			}
		}

		if (Params->TryGetStringField(TEXT("offsets"), OffsetsStr))
		{
			FMargin Offsets;
			if (ParseMargin(OffsetsStr, Offsets))
			{
				CanvasSlot->SetOffsets(Offsets);
				AppliedSettings += FString::Printf(TEXT("offsets=(%.0f,%.0f,%.0f,%.0f) "), Offsets.Left, Offsets.Top, Offsets.Right, Offsets.Bottom);
			}
		}

		if (Params->TryGetStringField(TEXT("alignment"), AlignmentStr))
		{
			FVector2D Alignment;
			if (ParseVector2D(AlignmentStr, Alignment))
			{
				CanvasSlot->SetAlignment(Alignment);
				AppliedSettings += FString::Printf(TEXT("alignment=(%.1f,%.1f) "), Alignment.X, Alignment.Y);
			}
		}

		if (Params->TryGetBoolField(TEXT("auto_size"), bAutoSize))
		{
			CanvasSlot->SetAutoSize(bAutoSize);
			AppliedSettings += FString::Printf(TEXT("auto_size=%s "), bAutoSize ? TEXT("true") : TEXT("false"));
		}

		if (Params->TryGetNumberField(TEXT("z_order"), ZOrder))
		{
			CanvasSlot->SetZOrder(ZOrder);
			AppliedSettings += FString::Printf(TEXT("z_order=%d "), ZOrder);
		}
	}

	// ====== VerticalBoxSlot ======
	UVerticalBoxSlot* VBSlot = Cast<UVerticalBoxSlot>(Widget->Slot);
	if (VBSlot)
	{
		FString PaddingStr, SizeRuleStr, HAlignStr, VAlignStr;

		if (Params->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				VBSlot->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (Params->TryGetStringField(TEXT("size_rule"), SizeRuleStr))
		{
			if (SizeRuleStr.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
			{
				VBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				AppliedSettings += TEXT("size=Fill ");
			}
			else
			{
				VBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				AppliedSettings += TEXT("size=Auto ");
			}
		}

		if (Params->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			VBSlot->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		if (Params->TryGetStringField(TEXT("v_align"), VAlignStr))
		{
			VBSlot->SetVerticalAlignment(ParseVAlign(VAlignStr));
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== HorizontalBoxSlot ======
	UHorizontalBoxSlot* HBSlot = Cast<UHorizontalBoxSlot>(Widget->Slot);
	if (HBSlot)
	{
		FString PaddingStr, SizeRuleStr, HAlignStr, VAlignStr;

		if (Params->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				HBSlot->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (Params->TryGetStringField(TEXT("size_rule"), SizeRuleStr))
		{
			if (SizeRuleStr.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
			{
				HBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				AppliedSettings += TEXT("size=Fill ");
			}
			else
			{
				HBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				AppliedSettings += TEXT("size=Auto ");
			}
		}

		if (Params->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			HBSlot->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		if (Params->TryGetStringField(TEXT("v_align"), VAlignStr))
		{
			HBSlot->SetVerticalAlignment(ParseVAlign(VAlignStr));
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== OverlaySlot ======
	UOverlaySlot* OverlaySlotPtr = Cast<UOverlaySlot>(Widget->Slot);
	if (OverlaySlotPtr)
	{
		FString PaddingStr, HAlignStr, VAlignStr;

		if (Params->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				OverlaySlotPtr->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (Params->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			OverlaySlotPtr->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		if (Params->TryGetStringField(TEXT("v_align"), VAlignStr))
		{
			OverlaySlotPtr->SetVerticalAlignment(ParseVAlign(VAlignStr));
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== GridSlot ======
	UGridSlot* GridSlotPtr = Cast<UGridSlot>(Widget->Slot);
	if (GridSlotPtr)
	{
		int32 Row = 0, Column = 0, RowSpan = 1, ColumnSpan = 1;
		FString PaddingStr, HAlignStr, VAlignStr;

		if (Params->TryGetNumberField(TEXT("row"), Row))
		{
			GridSlotPtr->SetRow(Row);
			AppliedSettings += FString::Printf(TEXT("row=%d "), Row);
		}
		if (Params->TryGetNumberField(TEXT("column"), Column))
		{
			GridSlotPtr->SetColumn(Column);
			AppliedSettings += FString::Printf(TEXT("column=%d "), Column);
		}
		if (Params->TryGetNumberField(TEXT("row_span"), RowSpan))
		{
			GridSlotPtr->SetRowSpan(RowSpan);
			AppliedSettings += FString::Printf(TEXT("row_span=%d "), RowSpan);
		}
		if (Params->TryGetNumberField(TEXT("column_span"), ColumnSpan))
		{
			GridSlotPtr->SetColumnSpan(ColumnSpan);
			AppliedSettings += FString::Printf(TEXT("column_span=%d "), ColumnSpan);
		}

		if (Params->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				GridSlotPtr->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (Params->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			GridSlotPtr->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}
		if (Params->TryGetStringField(TEXT("v_align"), VAlignStr))
		{
			GridSlotPtr->SetVerticalAlignment(ParseVAlign(VAlignStr));
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== ScrollBoxSlot ======
	UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(Widget->Slot);
	if (ScrollSlot)
	{
		FString PaddingStr, SizeRuleStr, HAlignStr;

		if (Params->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				ScrollSlot->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (Params->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			ScrollSlot->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}
	}

	// ====== WrapBoxSlot ======
	UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(Widget->Slot);
	if (WrapSlot)
	{
		FString PaddingStr, HAlignStr, VAlignStr;
		bool bFillEmptySpace = false;

		if (Params->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				WrapSlot->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (Params->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			WrapSlot->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		if (Params->TryGetStringField(TEXT("v_align"), VAlignStr))
		{
			WrapSlot->SetVerticalAlignment(ParseVAlign(VAlignStr));
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}

		if (Params->TryGetBoolField(TEXT("fill_empty_space"), bFillEmptySpace))
		{
			WrapSlot->SetFillEmptySpace(bFillEmptySpace);
			AppliedSettings += FString::Printf(TEXT("fill=%s "), bFillEmptySpace ? TEXT("true") : TEXT("false"));
		}
	}

	if (AppliedSettings.IsEmpty())
	{
		FString SlotType = Widget->Slot ? Widget->Slot->GetClass()->GetName() : TEXT("unknown");
		Result.Errors.Add(FString::Printf(TEXT("No slot properties were applied. Widget '%s' has slot type '%s'. Check that you're providing the correct properties for this slot type."), *WidgetName, *SlotType));
		return Result;
	}

	CompileAndMarkDirty(WidgetBP);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Configured slot on '%s': %s"), *WidgetName, *AppliedSettings.TrimEnd());
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteSetWidgetProperty
// ============================================================================

FAntigravityActionResult FAntigravityWidgetActions::ExecuteSetWidgetProperty(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath     = Params->GetStringField(TEXT("asset_path"));
	FString WidgetName    = Params->GetStringField(TEXT("widget_name"));
	FString PropertyName  = Params->GetStringField(TEXT("property_name"));
	FString PropertyValue = Params->GetStringField(TEXT("property_value"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

	UWidget* TargetWidget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!TargetWidget) return Result;

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

FAntigravityActionResult FAntigravityWidgetActions::ExecuteSetWidgetFont(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath  = Params->GetStringField(TEXT("asset_path"));
	FString WidgetName = Params->GetStringField(TEXT("widget_name"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!Widget) return Result;

	WidgetBP->Modify();
	Widget->Modify();

	if (ApplyWidgetFontHelper(WidgetBP, Widget, Params, Result))
	{
		CompileAndMarkDirty(WidgetBP);

		FSlateFontInfo FontInfo;
		UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
		if (TextBlock)
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

		FString FontFamily;
		Params->TryGetStringField(TEXT("font_family"), FontFamily);
		FString Typeface;
		Params->TryGetStringField(TEXT("typeface"), Typeface);

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

FAntigravityActionResult FAntigravityWidgetActions::ExecuteSetWidgetBrush(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath   = Params->GetStringField(TEXT("asset_path"));
	FString WidgetName  = Params->GetStringField(TEXT("widget_name"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!Widget) return Result;

	WidgetBP->Modify();
	Widget->Modify();

	if (ApplyWidgetBrushHelper(WidgetBP, Widget, Params, Result))
	{
		CompileAndMarkDirty(WidgetBP);

		FString BrushTarget = TEXT("Brush");
		Params->TryGetStringField(TEXT("brush_target"), BrushTarget);
		FString TexturePath;
		Params->TryGetStringField(TEXT("texture_path"), TexturePath);

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

FAntigravityActionResult FAntigravityWidgetActions::ExecuteBindWidgetEvent(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath  = Params->GetStringField(TEXT("asset_path"));
	FString WidgetName = Params->GetStringField(TEXT("widget_name"));
	FString EventName  = Params->GetStringField(TEXT("event_name"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!Widget) return Result;

	WidgetBP->Modify();

	if (ApplyWidgetEventHelper(WidgetBP, Widget, EventName, TEXT(""), Result))
	{
		CompileAndMarkDirty(WidgetBP);

		FString NodeName;
		int32 NodePosX = 0, NodePosY = 0;
		UEdGraph* EventGraph = nullptr;
		for (UEdGraph* Graph : WidgetBP->UbergraphPages)
		{
			if (Graph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
			{
				EventGraph = Graph;
				break;
			}
		}
		if (!EventGraph && WidgetBP->UbergraphPages.Num() > 0)
		{
			EventGraph = WidgetBP->UbergraphPages[0];
		}
		if (EventGraph)
		{
			for (UEdGraphNode* Node : EventGraph->Nodes)
			{
				UK2Node_ComponentBoundEvent* BoundNode = Cast<UK2Node_ComponentBoundEvent>(Node);
				if (BoundNode && BoundNode->ComponentPropertyName == FName(*WidgetName) && BoundNode->DelegatePropertyName == FName(*EventName))
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

FAntigravityActionResult FAntigravityWidgetActions::ExecuteRemoveWidget(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath  = Params->GetStringField(TEXT("asset_path"));
	FString WidgetName = Params->GetStringField(TEXT("widget_name"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Result);
	if (!Widget) return Result;

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();

	bool bWasRoot = (Widget == WidgetBP->WidgetTree->RootWidget);

	// If the widget has a parent panel, remove from it
	UPanelWidget* Parent = Widget->GetParent();
	if (Parent)
	{
		Parent->Modify();
		Parent->RemoveChild(Widget);
	}

	// If it was the root, clear the root
	if (bWasRoot)
	{
		WidgetBP->WidgetTree->RootWidget = nullptr;
		Result.Warnings.Add(TEXT("Removed the root widget. The widget tree is now empty. Use add_widget to add a new root."));
	}

	CompileAndMarkDirty(WidgetBP);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Removed widget '%s' from '%s'.%s"), *WidgetName, *AssetPath,
		bWasRoot ? TEXT(" (was root â€” tree is now empty)") : TEXT(""));
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteGetWidgetTree
// ============================================================================

FAntigravityActionResult FAntigravityWidgetActions::ExecuteGetWidgetTree(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

	FString Json = BuildWidgetTreeJson(WidgetBP);
	Result.bSuccess = true;
	Result.ResultMessage = Json;
	return Result;
}

// ============================================================================
// ExecuteCompileWidgetBlueprint
// ============================================================================

FAntigravityActionResult FAntigravityWidgetActions::ExecuteCompileWidgetBlueprint(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

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
	if (WidgetBP->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
		WidgetCount = AllWidgets.Num();
		if (WidgetBP->WidgetTree->RootWidget)
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

FAntigravityActionResult FAntigravityWidgetActions::ExecuteCaptureWidget(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

	UClass* WidgetClass = WidgetBP->GeneratedClass;
	if (!WidgetClass)
	{
		Result.Errors.Add(TEXT("Widget Blueprint has no GeneratedClass. Try compiling it first."));
		return Result;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Result.Errors.Add(TEXT("Could not find a valid World context to spawn the widget."));
		return Result;
	}

	UUserWidget* CreatedWidget = CreateWidget<UUserWidget>(World, WidgetClass);
	if (!CreatedWidget)
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
	if (Params->TryGetStringField(TEXT("render_size"), RenderSizeStr))
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
		Params->TryGetStringField(TEXT("resolution"), ResolutionStr);
		if (!ParseVector2D(ResolutionStr, RenderSize))
		{
			RenderSize = FVector2D(1920, 1080);
		}
	}

	int32 Width = FMath::Max(1, FMath::RoundToInt(RenderSize.X));
	int32 Height = FMath::Max(1, FMath::RoundToInt(RenderSize.Y));

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
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
	Params->TryGetNumberField(TEXT("max_dimension"), MaxDimension);

	int32 Quality = 75;
	Params->TryGetNumberField(TEXT("quality"), Quality);

	FString FilePath = FAntigravityViewportActions::SavePixelsToDisk(Pixels, Width, Height, MaxDimension, Quality);
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

bool FAntigravityWidgetActions::ApplyWidgetPropertyHelper(UWidgetBlueprint* WidgetBP, UWidget* TargetWidget, const FString& PropertyName, const FString& PropertyValue, FAntigravityActionResult& Result)
{
	if (!WidgetBP || !TargetWidget) return false;

	FString WidgetName = TargetWidget->GetName();

	// Special handling for Text property on TextBlock — use SetText for proper FText
	if (PropertyName == TEXT("Text"))
	{
		UTextBlock* TextBlock = Cast<UTextBlock>(TargetWidget);
		if (TextBlock)
		{
			TextBlock->Modify();
			TextBlock->SetText(FText::FromString(PropertyValue));
			return true;
		}

		// Also handle EditableTextBox Text
		UEditableTextBox* EditBox = Cast<UEditableTextBox>(TargetWidget);
		if (EditBox)
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
		if (EditBox)
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

	UE_LOG(LogAntigravity, Log, TEXT("WidgetActions: Set property '%s' = '%s' on widget '%s'"), *PropertyName, *PropertyValue, *WidgetName);

	return true;
}

bool FAntigravityWidgetActions::ApplyWidgetFontHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& FontParams, FAntigravityActionResult& Result)
{
	if (!WidgetBP || !Widget || !FontParams.IsValid()) return false;

	FString WidgetName = Widget->GetName();

	// Get the current font info from the widget
	FSlateFontInfo FontInfo;

	UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
	if (TextBlock)
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
	if (FontParams->TryGetStringField(TEXT("font_family"), FontFamily) && !FontFamily.IsEmpty())
	{
		if (FontFamily.Equals(TEXT("Roboto"), ESearchCase::IgnoreCase))
		{
			UObject* FontObj = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
			if (FontObj)
			{
				FontInfo.FontObject = FontObj;
			}
		}
		else if (FontFamily.Equals(TEXT("DroidSansMono"), ESearchCase::IgnoreCase))
		{
			UObject* FontObj = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono"));
			if (FontObj)
			{
				FontInfo.FontObject = FontObj;
			}
		}
		else if (FontFamily.StartsWith(TEXT("/Game/")) || FontFamily.StartsWith(TEXT("/Engine/")))
		{
			UObject* FontObj = LoadObject<UFont>(nullptr, *FontFamily);
			if (FontObj)
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
	if (FontParams->TryGetNumberField(TEXT("font_size"), FontSize))
	{
		FontInfo.Size = FontSize;
	}

	// Apply typeface
	FString Typeface;
	if (FontParams->TryGetStringField(TEXT("typeface"), Typeface) && !Typeface.IsEmpty())
	{
		FontInfo.TypefaceFontName = FName(*Typeface);
	}

	// Set the font back on the widget
	if (TextBlock)
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
	if (TextBlock && FontParams->TryGetStringField(TEXT("color"), ColorStr))
	{
		FLinearColor Color;
		if (ParseLinearColor(ColorStr, Color))
		{
			TextBlock->SetColorAndOpacity(FSlateColor(Color));
		}
	}

	// Apply shadow
	FString ShadowOffsetStr;
	if (TextBlock && FontParams->TryGetStringField(TEXT("shadow_offset"), ShadowOffsetStr))
	{
		FVector2D ShadowOffset;
		if (ParseVector2D(ShadowOffsetStr, ShadowOffset))
		{
			TextBlock->SetShadowOffset(ShadowOffset);
		}
	}

	FString ShadowColorStr;
	if (TextBlock && FontParams->TryGetStringField(TEXT("shadow_color"), ShadowColorStr))
	{
		FLinearColor ShadowColor;
		if (ParseLinearColor(ShadowColorStr, ShadowColor))
		{
			TextBlock->SetShadowColorAndOpacity(ShadowColor);
		}
	}

	return true;
}

bool FAntigravityWidgetActions::ApplyWidgetBrushHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& BrushParams, FAntigravityActionResult& Result)
{
	if (!WidgetBP || !Widget || !BrushParams.IsValid()) return false;

	FString WidgetName = Widget->GetName();

	FString BrushTarget = TEXT("Brush");
	BrushParams->TryGetStringField(TEXT("brush_target"), BrushTarget);

	// Build the brush
	FSlateBrush NewBrush;

	// Load texture if provided
	FString TexturePath;
	if (BrushParams->TryGetStringField(TEXT("texture_path"), TexturePath) && !TexturePath.IsEmpty())
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
		if (Texture)
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
	if (BrushParams->TryGetStringField(TEXT("tint_color"), TintColorStr))
	{
		FLinearColor TintColor;
		if (ParseLinearColor(TintColorStr, TintColor))
		{
			NewBrush.TintColor = FSlateColor(TintColor);
		}
	}

	// Set image size
	FString ImageSizeStr;
	if (BrushParams->TryGetStringField(TEXT("image_size"), ImageSizeStr))
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
	if (BrushParams->TryGetStringField(TEXT("draw_as"), DrawAsStr))
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
	if (BrushParams->TryGetStringField(TEXT("margin"), MarginStr))
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
	if (ImageWidget && BrushTarget.Equals(TEXT("Brush"), ESearchCase::IgnoreCase))
	{
		ImageWidget->SetBrush(NewBrush);
		bApplied = true;
	}

	// === Button widget ===
	UButton* ButtonWidget = Cast<UButton>(Widget);
	if (ButtonWidget)
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
	if (BorderWidget && BrushTarget.Equals(TEXT("Background"), ESearchCase::IgnoreCase))
	{
		BorderWidget->SetBrush(NewBrush);
		bApplied = true;
	}

	// === ProgressBar widget ===
	UProgressBar* PBWidget = Cast<UProgressBar>(Widget);
	if (PBWidget && BrushTarget.Equals(TEXT("FillImage"), ESearchCase::IgnoreCase))
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

bool FAntigravityWidgetActions::ApplyWidgetEventHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const FString& EventName, const FString& FunctionName, FAntigravityActionResult& Result)
{
	if (!WidgetBP || !Widget) return false;

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
		if (Graph->GetFName() == UEdGraphSchema_K2::GN_EventGraph)
		{
			EventGraph = Graph;
			break;
		}
	}

	if (!EventGraph && WidgetBP->UbergraphPages.Num() > 0)
	{
		EventGraph = WidgetBP->UbergraphPages[0];
	}

	if (!EventGraph)
	{
		Result.Errors.Add(TEXT("Widget Blueprint has no EventGraph. This is unexpected — try recompiling the Widget Blueprint first."));
		return false;
	}

	EventGraph->Modify();

	// Create a K2Node_ComponentBoundEvent
	UK2Node_ComponentBoundEvent* EventNode = NewObject<UK2Node_ComponentBoundEvent>(EventGraph);
	EventNode->DelegatePropertyName = DelegateProp->GetFName();
	EventNode->DelegateOwnerClass = Widget->GetClass();
	EventNode->ComponentPropertyName = FName(*WidgetName);

	// Position the node (find an empty area)
	int32 MaxY = 0;
	for (UEdGraphNode* Node : EventGraph->Nodes)
	{
		if (Node)
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

bool FAntigravityWidgetActions::ApplyWidgetSlotHelper(UWidgetBlueprint* WidgetBP, UWidget* Widget, const TSharedPtr<FJsonObject>& SlotParams, FAntigravityActionResult& Result)
{
	if (!WidgetBP || !Widget || !SlotParams.IsValid()) return false;

	FString WidgetName = Widget->GetName();

	if (!Widget->Slot)
	{
		Result.Errors.Add(FString::Printf(TEXT("Widget '%s' has no slot. It may be the root widget (root widgets don't have slots) or not yet attached to a parent panel. Add it to a panel first via add_widget."), *WidgetName));
		return false;
	}

	Widget->Slot->Modify();

	FString AppliedSettings;

	// ====== CanvasPanelSlot ======
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (CanvasSlot)
	{
		FString AnchorsMinStr, AnchorsMaxStr, OffsetsStr, AlignmentStr;
		bool bAutoSize = false;
		int32 ZOrder = 0;

		if (SlotParams->TryGetStringField(TEXT("anchors_min"), AnchorsMinStr))
		{
			FVector2D AnchorMin;
			if (ParseVector2D(AnchorsMinStr, AnchorMin))
			{
				FAnchors Anchors = CanvasSlot->GetAnchors();
				Anchors.Minimum = AnchorMin;

				if (SlotParams->TryGetStringField(TEXT("anchors_max"), AnchorsMaxStr))
				{
					FVector2D AnchorMax;
					if (ParseVector2D(AnchorsMaxStr, AnchorMax))
					{
						Anchors.Maximum = AnchorMax;
					}
				}
				CanvasSlot->SetAnchors(Anchors);
				AppliedSettings += FString::Printf(TEXT("anchors=(%.1f,%.1f)-(%.1f,%.1f) "), Anchors.Minimum.X, Anchors.Minimum.Y, Anchors.Maximum.X, Anchors.Maximum.Y);
			}
		}
		else if (SlotParams->TryGetStringField(TEXT("anchors_max"), AnchorsMaxStr))
		{
			FVector2D AnchorMax;
			if (ParseVector2D(AnchorsMaxStr, AnchorMax))
			{
				FAnchors Anchors = CanvasSlot->GetAnchors();
				Anchors.Maximum = AnchorMax;
				CanvasSlot->SetAnchors(Anchors);
				AppliedSettings += FString::Printf(TEXT("anchors_max=(%.1f,%.1f) "), AnchorMax.X, AnchorMax.Y);
			}
		}

		if (SlotParams->TryGetStringField(TEXT("offsets"), OffsetsStr))
		{
			FMargin Offsets;
			if (ParseMargin(OffsetsStr, Offsets))
			{
				CanvasSlot->SetOffsets(Offsets);
				AppliedSettings += FString::Printf(TEXT("offsets=(%.0f,%.0f,%.0f,%.0f) "), Offsets.Left, Offsets.Top, Offsets.Right, Offsets.Bottom);
			}
		}

		if (SlotParams->TryGetStringField(TEXT("alignment"), AlignmentStr))
		{
			FVector2D Alignment;
			if (ParseVector2D(AlignmentStr, Alignment))
			{
				CanvasSlot->SetAlignment(Alignment);
				AppliedSettings += FString::Printf(TEXT("alignment=(%.1f,%.1f) "), Alignment.X, Alignment.Y);
			}
		}

		if (SlotParams->TryGetBoolField(TEXT("auto_size"), bAutoSize))
		{
			CanvasSlot->SetAutoSize(bAutoSize);
			AppliedSettings += FString::Printf(TEXT("auto_size=%s "), bAutoSize ? TEXT("true") : TEXT("false"));
		}

		if (SlotParams->TryGetNumberField(TEXT("z_order"), ZOrder))
		{
			CanvasSlot->SetZOrder(ZOrder);
			AppliedSettings += FString::Printf(TEXT("z_order=%d "), ZOrder);
		}
	}

	// ====== VerticalBoxSlot ======
	UVerticalBoxSlot* VBSlot = Cast<UVerticalBoxSlot>(Widget->Slot);
	if (VBSlot)
	{
		FString PaddingStr, SizeRuleStr, HAlignStr, VAlignStr;

		if (SlotParams->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				VBSlot->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (SlotParams->TryGetStringField(TEXT("size_rule"), SizeRuleStr))
		{
			if (SizeRuleStr.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
			{
				VBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				AppliedSettings += TEXT("size=Fill ");
			}
			else
			{
				VBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				AppliedSettings += TEXT("size=Auto ");
			}
		}

		if (SlotParams->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			VBSlot->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		if (SlotParams->TryGetStringField(TEXT("v_align"), VAlignStr))
		{
			VBSlot->SetVerticalAlignment(ParseVAlign(VAlignStr));
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== HorizontalBoxSlot ======
	UHorizontalBoxSlot* HBSlot = Cast<UHorizontalBoxSlot>(Widget->Slot);
	if (HBSlot)
	{
		FString PaddingStr, SizeRuleStr, HAlignStr, VAlignStr;

		if (SlotParams->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				HBSlot->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (SlotParams->TryGetStringField(TEXT("size_rule"), SizeRuleStr))
		{
			if (SizeRuleStr.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
			{
				HBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				AppliedSettings += TEXT("size=Fill ");
			}
			else
			{
				HBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				AppliedSettings += TEXT("size=Auto ");
			}
		}

		if (SlotParams->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			HBSlot->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		if (SlotParams->TryGetStringField(TEXT("v_align"), VAlignStr))
		{
			HBSlot->SetVerticalAlignment(ParseVAlign(VAlignStr));
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== OverlaySlot ======
	UOverlaySlot* OverlaySlotPtr = Cast<UOverlaySlot>(Widget->Slot);
	if (OverlaySlotPtr)
	{
		FString PaddingStr, HAlignStr, VAlignStr;

		if (SlotParams->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				OverlaySlotPtr->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (SlotParams->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			OverlaySlotPtr->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		if (SlotParams->TryGetStringField(TEXT("v_align"), VAlignStr))
		{
			OverlaySlotPtr->SetVerticalAlignment(ParseVAlign(VAlignStr));
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== GridSlot ======
	UGridSlot* GridSlotPtr = Cast<UGridSlot>(Widget->Slot);
	if (GridSlotPtr)
	{
		int32 Row = 0, Column = 0, RowSpan = 1, ColumnSpan = 1;
		FString PaddingStr, HAlignStr, VAlignStr;

		if (SlotParams->TryGetNumberField(TEXT("row"), Row))
		{
			GridSlotPtr->SetRow(Row);
			AppliedSettings += FString::Printf(TEXT("row=%d "), Row);
		}
		if (SlotParams->TryGetNumberField(TEXT("column"), Column))
		{
			GridSlotPtr->SetColumn(Column);
			AppliedSettings += FString::Printf(TEXT("column=%d "), Column);
		}
		if (SlotParams->TryGetNumberField(TEXT("row_span"), RowSpan))
		{
			GridSlotPtr->SetRowSpan(RowSpan);
			AppliedSettings += FString::Printf(TEXT("row_span=%d "), RowSpan);
		}
		if (SlotParams->TryGetNumberField(TEXT("column_span"), ColumnSpan))
		{
			GridSlotPtr->SetColumnSpan(ColumnSpan);
			AppliedSettings += FString::Printf(TEXT("column_span=%d "), ColumnSpan);
		}

		if (SlotParams->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				GridSlotPtr->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (SlotParams->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			GridSlotPtr->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}
		if (SlotParams->TryGetStringField(TEXT("v_align"), VAlignStr))
		{
			GridSlotPtr->SetVerticalAlignment(ParseVAlign(VAlignStr));
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}
	}

	// ====== ScrollBoxSlot ======
	UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(Widget->Slot);
	if (ScrollSlot)
	{
		FString PaddingStr, SizeRuleStr, HAlignStr;

		if (SlotParams->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				ScrollSlot->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (SlotParams->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			ScrollSlot->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}
	}

	// ====== WrapBoxSlot ======
	UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(Widget->Slot);
	if (WrapSlot)
	{
		FString PaddingStr, HAlignStr, VAlignStr;
		bool bFillEmptySpace = false;

		if (SlotParams->TryGetStringField(TEXT("padding"), PaddingStr))
		{
			FMargin Padding;
			if (ParseMargin(PaddingStr, Padding))
			{
				WrapSlot->SetPadding(Padding);
				AppliedSettings += FString::Printf(TEXT("padding=(%.0f,%.0f,%.0f,%.0f) "), Padding.Left, Padding.Top, Padding.Right, Padding.Bottom);
			}
		}

		if (SlotParams->TryGetStringField(TEXT("h_align"), HAlignStr))
		{
			WrapSlot->SetHorizontalAlignment(ParseHAlign(HAlignStr));
			AppliedSettings += FString::Printf(TEXT("h_align=%s "), *HAlignStr);
		}

		if (SlotParams->TryGetStringField(TEXT("v_align"), VAlignStr))
		{
			WrapSlot->SetVerticalAlignment(ParseVAlign(VAlignStr));
			AppliedSettings += FString::Printf(TEXT("v_align=%s "), *VAlignStr);
		}

		if (SlotParams->TryGetBoolField(TEXT("fill_empty_space"), bFillEmptySpace))
		{
			WrapSlot->SetFillEmptySpace(bFillEmptySpace);
			AppliedSettings += FString::Printf(TEXT("fill=%s "), bFillEmptySpace ? TEXT("true") : TEXT("false"));
		}
	}

	if (AppliedSettings.IsEmpty())
	{
		FString SlotType = Widget->Slot ? Widget->Slot->GetClass()->GetName() : TEXT("unknown");
		Result.Errors.Add(FString::Printf(TEXT("No slot properties were applied. Widget '%s' has slot type '%s'. Check that you're providing the correct properties for this slot type."), *WidgetName, *SlotType));
		return false;
	}

	return true;
}

FAntigravityActionResult FAntigravityWidgetActions::ExecuteInstantiateUIHierarchy(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	UWidgetBlueprint* WidgetBP = LoadWidgetBP(AssetPath, Result);
	if (!WidgetBP) return Result;

	if (!WidgetBP->WidgetTree)
	{
		Result.Errors.Add(TEXT("Widget Blueprint has no WidgetTree — recreate the asset."));
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* WidgetsArray = nullptr;
	if (!Params->TryGetArrayField(TEXT("widgets"), WidgetsArray))
	{
		Result.Errors.Add(TEXT("Missing required array field: widgets"));
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
		if (!WidgetObj->TryGetStringField(TEXT("widget_class"), WidgetClassName) ||
			!WidgetObj->TryGetStringField(TEXT("widget_name"), WidgetName))
		{
			Result.Warnings.Add(TEXT("Skipping widget: missing widget_class or widget_name."));
			continue;
		}

		// Resolve class
		UClass* WidgetClass = ResolveWidgetClass(WidgetClassName);
		if (!WidgetClass)
		{
			Result.Errors.Add(FString::Printf(TEXT("Widget class not found: '%s' for widget '%s'."), *WidgetClassName, *WidgetName));
			return Result;
		}

		// Construct widget
		UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
		if (!NewWidget)
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to construct widget '%s' of class '%s'."), *WidgetName, *WidgetClassName));
			return Result;
		}

		WidgetBP->WidgetVariableNameToGuidMap.Add(NewWidget->GetFName(), FGuid::NewGuid());
		AddedCount++;

		// Attach to parent panel if specified
		FString ParentWidgetName;
		bool bAddedToParent = false;
		if (WidgetObj->TryGetStringField(TEXT("parent_widget"), ParentWidgetName) && !ParentWidgetName.IsEmpty())
		{
			UWidget* ParentWidgetRaw = WidgetBP->WidgetTree->FindWidget(FName(*ParentWidgetName));
			UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidgetRaw);
			if (ParentPanel)
			{
				ParentPanel->AddChild(NewWidget);
				bAddedToParent = true;
			}
			else if (ParentWidgetRaw)
			{
				UContentWidget* ContentParent = Cast<UContentWidget>(ParentWidgetRaw);
				if (ContentParent)
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
		if (!bAddedToParent && !WidgetBP->WidgetTree->RootWidget)
		{
			WidgetBP->WidgetTree->RootWidget = NewWidget;
		}
		else if (!bAddedToParent)
		{
			// Try attaching to root panel if root is a panel
			UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
			if (RootPanel)
			{
				RootPanel->AddChild(NewWidget);
			}
		}

		// Apply properties
		const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
		if (WidgetObj->TryGetObjectField(TEXT("properties"), PropertiesObj))
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
		if (WidgetObj->TryGetObjectField(TEXT("slot"), SlotObj))
		{
			if (ApplyWidgetSlotHelper(WidgetBP, NewWidget, *SlotObj, Result))
			{
				ConfiguredSlots++;
			}
		}

		// Apply font settings
		const TSharedPtr<FJsonObject>* FontObj = nullptr;
		if (WidgetObj->TryGetObjectField(TEXT("font"), FontObj))
		{
			if (ApplyWidgetFontHelper(WidgetBP, NewWidget, *FontObj, Result))
			{
				ConfiguredFonts++;
			}
		}

		// Apply brush settings
		const TSharedPtr<FJsonObject>* BrushObj = nullptr;
		if (WidgetObj->TryGetObjectField(TEXT("brush"), BrushObj))
		{
			if (ApplyWidgetBrushHelper(WidgetBP, NewWidget, *BrushObj, Result))
			{
				ConfiguredBrushes++;
			}
		}

		// Apply event bindings
		const TSharedPtr<FJsonObject>* EventsObj = nullptr;
		if (WidgetObj->TryGetObjectField(TEXT("events"), EventsObj))
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
