// Copyright 2026 AgentFramework. All Rights Reserved.

#include "PIE/AgentFrameworkPIEActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkSettings.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AgentFrameworkPIEActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAgentFrameworkPIEActions::FAgentFrameworkPIEActions() {}
FAgentFrameworkPIEActions::~FAgentFrameworkPIEActions() {}

// ============================================================================
// IAgentFrameworkActionExecutor Interface
// ============================================================================

FName FAgentFrameworkPIEActions::GetActionName() const { return FName(TEXT("PIE")); }

TArray<FString> FAgentFrameworkPIEActions::GetSupportedToolNames() const
{
	return {
		TEXT("start_pie_session"),
		TEXT("simulate_input"),
		TEXT("stop_pie_session"),
		TEXT("extract_ui_state"),
		TEXT("trigger_ui_element"),
		TEXT("query_world_state")
	};
}

bool FAgentFrameworkPIEActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkPIEActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	// Security gate: require Full Access mode
	const UAgentFrameworkDeveloperSettings* Settings = UAgentFrameworkDeveloperSettings::Get();
	if (!Settings || Settings->SecurityMode != EAgentFrameworkSecurityMode::FullAccess)
	{
		Result.Errors.Add(TEXT("PIE automation requires Full Access security mode."));
		return Result;
	}

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty())
		Params->TryGetStringField(TEXT("tool_name"), Action);

	if (Action == TEXT("start_pie_session"))
		return ExecuteStartPIE(Params, Result);
	else if (Action == TEXT("simulate_input"))
		return ExecuteSimulateInput(Params, Result);
	else if (Action == TEXT("stop_pie_session"))
		return ExecuteStopPIE(Params, Result);
	else if (Action == TEXT("extract_ui_state"))
		return ExecuteExtractUIState(Params, Result);
	else if (Action == TEXT("trigger_ui_element"))
		return ExecuteTriggerUIElement(Params, Result);
	else if (Action == TEXT("query_world_state"))
		return ExecuteQueryWorldState(Params, Result);

	Result.Errors.Add(TEXT("Unknown PIE action. Use start_pie_session, simulate_input, stop_pie_session, extract_ui_state, trigger_ui_element, or query_world_state."));
	return Result;
}

// ============================================================================
// start_pie_session
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPIEActions::ExecuteStartPIE(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (IsPIERunning())
	{
		Result.Warnings.Add(TEXT("PIE is already running."));
		Result.bSuccess = true;
		Result.ResultMessage = TEXT("PIE session is already active. Use simulate_input to interact, or stop_pie_session to end it.");
		return Result;
	}

	if (!GUnrealEd)
	{
		Result.Errors.Add(TEXT("GUnrealEd is not available."));
		return Result;
	}

	// Configure PIE settings
	FRequestPlaySessionParams PlayParams;

	// Optional: set the start location
	FString StartMode;
	Params->TryGetStringField(TEXT("start_mode"), StartMode);

	if (StartMode.Equals(TEXT("current_camera"), ESearchCase::IgnoreCase))
	{
		PlayParams.EditorPlaySettings = NewObject<ULevelEditorPlaySettings>();
		PlayParams.EditorPlaySettings->LastExecutedPlayModeLocation = PlayLocation_CurrentCameraLocation;
	}

	// Request PIE session
	GUnrealEd->RequestPlaySession(PlayParams);

	UE_LOG(LogAgentFramework, Log, TEXT("PIEActions: Requested PIE session start."));

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("PIE session started. The game is now running in the editor. "
		"Use read_message_log to check for runtime errors. "
		"Use simulate_input to inject key presses. "
		"Use stop_pie_session to end the session.");
	return Result;
}

// ============================================================================
// simulate_input
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPIEActions::ExecuteSimulateInput(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!IsPIERunning())
	{
		Result.Errors.Add(TEXT("PIE is not running. Call start_pie_session first."));
		return Result;
	}

	FString KeyName;
	if (!Params->TryGetStringField(TEXT("key"), KeyName))
	{
		Result.Errors.Add(TEXT("Missing required field: 'key' (e.g., 'W', 'SpaceBar', 'LeftMouseButton', 'Gamepad_FaceButton_Bottom')"));
		return Result;
	}

	FString ActionType;
	if (!Params->TryGetStringField(TEXT("action_type"), ActionType))
	{
		ActionType = TEXT("press"); // Default: press and release
	}
	ActionType = ActionType.ToLower();

	float DurationSeconds = 0.0f;
	Params->TryGetNumberField(TEXT("duration"), DurationSeconds);
	DurationSeconds = FMath::Clamp(DurationSeconds, 0.0f, 10.0f);

	// Resolve the FKey
	FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Invalid key name: '%s'. Use UE key names like 'W', 'A', 'S', 'D', 'SpaceBar', 'LeftShift', "
				 "'LeftMouseButton', 'Gamepad_FaceButton_Bottom', 'Gamepad_LeftStick_Up', etc."),
			*KeyName));
		return Result;
	}

	FString Report;

	if (ActionType == TEXT("press") || ActionType == TEXT("tap"))
	{
		// Simulate press + release
		FSlateApplication& Slate = FSlateApplication::Get();
		FKeyEvent KeyEvent(Key, FModifierKeysState(), 0, false, 0, 0);

		Slate.ProcessKeyDownEvent(KeyEvent);

		if (DurationSeconds > 0.0f)
		{
			// For duration-based press, we'd need async. For now, just hold for one frame.
			// The AI should use multiple simulate_input calls for sustained input.
			Report = FString::Printf(TEXT("Pressed '%s' (note: sustained press for %.1fs requires multiple calls or PIE tick integration). "
				"Key was pressed and released in a single frame."), *KeyName, DurationSeconds);
		}
		else
		{
			Report = FString::Printf(TEXT("Tapped '%s' (press + release in one frame)."), *KeyName);
		}

		Slate.ProcessKeyUpEvent(KeyEvent);
	}
	else if (ActionType == TEXT("down") || ActionType == TEXT("hold"))
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		FKeyEvent KeyEvent(Key, FModifierKeysState(), 0, false, 0, 0);
		Slate.ProcessKeyDownEvent(KeyEvent);
		Report = FString::Printf(TEXT("Pressed '%s' down (held). Use action_type='up' to release."), *KeyName);
	}
	else if (ActionType == TEXT("up") || ActionType == TEXT("release"))
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		FKeyEvent KeyEvent(Key, FModifierKeysState(), 0, false, 0, 0);
		Slate.ProcessKeyUpEvent(KeyEvent);
		Report = FString::Printf(TEXT("Released '%s'."), *KeyName);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown action_type '%s'. Use 'press', 'down', or 'up'."), *ActionType));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = Report;
	return Result;
}

// ============================================================================
// stop_pie_session
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPIEActions::ExecuteStopPIE(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!IsPIERunning())
	{
		Result.bSuccess = true;
		Result.ResultMessage = TEXT("PIE is not running. Nothing to stop.");
		return Result;
	}

	if (GUnrealEd)
	{
		GUnrealEd->RequestEndPlayMap();
		UE_LOG(LogAgentFramework, Log, TEXT("PIEActions: Requested PIE session stop."));
	}

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("PIE session stopped. Use read_message_log to check for any runtime errors that occurred during play.");
	return Result;
}

// ============================================================================
// Helpers
// ============================================================================

bool FAgentFrameworkPIEActions::IsPIERunning()
{
	if (!GEditor) return false;
	return GEditor->IsPlaySessionInProgress();
}

// Helpers
static FString GetWidgetText(UWidget* Widget)
{
	if (!Widget) return TEXT("");
	
	FProperty* TextProp = Widget->GetClass()->FindPropertyByName(TEXT("Text"));
	if (TextProp)
	{
		if (FTextProperty* TextProperty = CastField<FTextProperty>(TextProp))
		{
			FText TextValue = TextProperty->GetPropertyValue_InContainer(Widget);
			return TextValue.ToString();
		}
		else if (FStrProperty* StrProperty = CastField<FStrProperty>(TextProp))
		{
			return StrProperty->GetPropertyValue_InContainer(Widget);
		}
		else if (FNameProperty* NameProperty = CastField<FNameProperty>(TextProp))
		{
			return NameProperty->GetPropertyValue_InContainer(Widget).ToString();
		}
	}
	
	return TEXT("");
}

static FString FindTextInWidget(UWidget* Widget)
{
	if (!Widget) return TEXT("");
	FString FoundText = GetWidgetText(Widget);
	if (!FoundText.IsEmpty()) return FoundText;

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			FoundText = FindTextInWidget(Panel->GetChildAt(i));
			if (!FoundText.IsEmpty()) return FoundText;
		}
	}
	return TEXT("");
}

static void FindSlateWidgetsRecursive(const TSharedRef<SWidget>& Widget, TArray<TSharedRef<SWidget>>& OutWidgets)
{
	OutWidgets.Add(Widget);
	FChildren* Children = Widget->GetChildren();
	for (int32 i = 0; i < Children->Num(); ++i)
	{
		FindSlateWidgetsRecursive(Children->GetChildAt(i), OutWidgets);
	}
}

static bool IsSlateInteractable(const TSharedRef<SWidget>& Widget)
{
	FString TypeName = Widget->GetTypeAsString();
	return TypeName.Contains(TEXT("Button")) || TypeName.Contains(TEXT("CheckBox")) || TypeName.Contains(TEXT("Hyperlink"));
}

static FString GetSlateWidgetText(const TSharedRef<SWidget>& Widget)
{
	FString TypeName = Widget->GetTypeAsString();
	if (TypeName == TEXT("STextBlock"))
	{
		const STextBlock* TextBlock = static_cast<const STextBlock*>(&Widget.Get());
		return TextBlock->GetText().ToString();
	}
	
	FChildren* Children = Widget->GetChildren();
	for (int32 i = 0; i < Children->Num(); ++i)
	{
		FString ChildText = GetSlateWidgetText(Children->GetChildAt(i));
		if (!ChildText.IsEmpty()) return ChildText;
	}
	return TEXT("");
}

FAgentFrameworkActionResult FAgentFrameworkPIEActions::ExecuteExtractUIState(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!IsPIERunning())
	{
		Result.Errors.Add(TEXT("PIE is not running."));
		return Result;
	}

	UWorld* World = nullptr;
	if (GEditor)
	{
		FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
		if (PIEWorldContext)
		{
			World = PIEWorldContext->World();
		}
	}
	if (!World)
	{
		Result.Errors.Add(TEXT("Could not find active PIE world."));
		return Result;
	}

	TSharedRef<FJsonObject> UMGStateJson = MakeShared<FJsonObject>();
	
	// Clear previous cache for raw Slate widgets
	CachedSlateWidgets.Empty();
	int32 SlateIndex = 0;

	// 1. Extract UMG widgets
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* UserWidget = *It;
		if (UserWidget->GetWorld() == World && UserWidget->IsInViewport() && UserWidget->WidgetTree)
		{
			FString WidgetName = UserWidget->GetName();
			
			UserWidget->WidgetTree->ForEachWidget([&](UWidget* ChildWidget) {
				if (ChildWidget && ChildWidget->IsVisible())
				{
					FString TypeName = ChildWidget->GetClass()->GetName();
					
					bool bIsInteractable = TypeName.Contains(TEXT("Button")) || 
										   TypeName.Contains(TEXT("CheckBox")) || 
										   TypeName.Contains(TEXT("EditableText")) ||
										   TypeName.Contains(TEXT("Slider"));
										   
					if (bIsInteractable)
					{
						FString Path = FString::Printf(TEXT("%s.%s"), *WidgetName, *ChildWidget->GetName());
						
						TSharedRef<FJsonObject> ElementJson = MakeShared<FJsonObject>();
						ElementJson->SetStringField(TEXT("type"), TypeName);
						
						FString TextContent = FindTextInWidget(ChildWidget);
						if (!TextContent.IsEmpty())
						{
							ElementJson->SetStringField(TEXT("text"), TextContent);
						}
						
						TSharedPtr<SWidget> SlateWidget = ChildWidget->GetCachedWidget();
						if (SlateWidget.IsValid())
						{
							FGeometry Geometry = SlateWidget->GetTickSpaceGeometry();
							FVector2D Center = Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize() * 0.5f;
							ElementJson->SetNumberField(TEXT("center_x"), Center.X);
							ElementJson->SetNumberField(TEXT("center_y"), Center.Y);
						}
						
						UMGStateJson->SetObjectField(Path, ElementJson);
					}
				}
			});
		}
	}
	
	TSharedRef<FJsonObject> SlateStateJson = MakeShared<FJsonObject>();
	
	// 2. Extract raw Slate widgets
	TArray<TSharedRef<SWindow>> Windows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
	
	TArray<TSharedRef<SWidget>> AllSlateWidgets;
	for (const TSharedRef<SWindow>& Window : Windows)
	{
		FindSlateWidgetsRecursive(Window, AllSlateWidgets);
	}
	
	for (const TSharedRef<SWidget>& Widget : AllSlateWidgets)
	{
		FString TypeName = Widget->GetTypeAsString();
		if (IsSlateInteractable(Widget))
		{
			if (TypeName.Equals(TEXT("SObjectWidget")) || TypeName.Contains(TEXT("UMG")))
			{
				continue;
			}
			
			FString SlateId = FString::Printf(TEXT("slate_%s_%d"), *TypeName.ToLower(), SlateIndex++);
			CachedSlateWidgets.Add(SlateId, Widget);
			
			TSharedRef<FJsonObject> ElementJson = MakeShared<FJsonObject>();
			ElementJson->SetStringField(TEXT("type"), TypeName);
			
			FString TextContent = GetSlateWidgetText(Widget);
			if (!TextContent.IsEmpty())
			{
				ElementJson->SetStringField(TEXT("text"), TextContent);
			}
			
			FGeometry Geometry = Widget->GetTickSpaceGeometry();
			FVector2D Center = Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize() * 0.5f;
			ElementJson->SetNumberField(TEXT("center_x"), Center.X);
			ElementJson->SetNumberField(TEXT("center_y"), Center.Y);
			
			SlateStateJson->SetObjectField(SlateId, ElementJson);
		}
	}
	
	TSharedRef<FJsonObject> ReturnJson = MakeShared<FJsonObject>();
	ReturnJson->SetObjectField(TEXT("umg"), UMGStateJson);
	ReturnJson->SetObjectField(TEXT("slate"), SlateStateJson);
	
	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ReturnJson, Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseString;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkPIEActions::ExecuteTriggerUIElement(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!IsPIERunning())
	{
		Result.Errors.Add(TEXT("PIE is not running."));
		return Result;
	}

	UWorld* World = nullptr;
	if (GEditor)
	{
		FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
		if (PIEWorldContext)
		{
			World = PIEWorldContext->World();
		}
	}
	if (!World)
	{
		Result.Errors.Add(TEXT("Could not find active PIE world."));
		return Result;
	}

	FString WidgetPath;
	if (!Params->TryGetStringField(TEXT("widget_path"), WidgetPath) || WidgetPath.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing required parameter: widget_path"));
		return Result;
	}

	TSharedPtr<SWidget> TargetSlateWidget;

	if (WidgetPath.StartsWith(TEXT("slate_")))
	{
		TWeakPtr<SWidget>* FoundPtr = CachedSlateWidgets.Find(WidgetPath);
		if (FoundPtr && FoundPtr->IsValid())
		{
			TargetSlateWidget = FoundPtr->Pin();
		}
		else
		{
			Result.Errors.Add(FString::Printf(TEXT("Slate widget ID '%s' is no longer valid or was not cached. Call extract_ui_state first."), *WidgetPath));
			return Result;
		}
	}
	else
	{
		FString WidgetName;
		FString ChildWidgetName;
		if (!WidgetPath.Split(TEXT("."), &WidgetName, &ChildWidgetName))
		{
			Result.Errors.Add(TEXT("Invalid widget_path format. Expected UserWidgetInstanceName.ChildWidgetName"));
			return Result;
		}

		UUserWidget* TargetUserWidget = nullptr;
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsInViewport() && It->GetName() == WidgetName)
			{
				TargetUserWidget = *It;
				break;
			}
		}

		if (!TargetUserWidget)
		{
			Result.Errors.Add(FString::Printf(TEXT("UserWidget '%s' not found or not in viewport."), *WidgetName));
			return Result;
		}

		UWidget* TargetWidget = TargetUserWidget->WidgetTree->FindWidget(*ChildWidgetName);
		if (!TargetWidget)
		{
			Result.Errors.Add(FString::Printf(TEXT("Child widget '%s' not found in '%s'."), *ChildWidgetName, *WidgetName));
			return Result;
		}

		TargetSlateWidget = TargetWidget->GetCachedWidget();
		if (!TargetSlateWidget.IsValid())
		{
			Result.Errors.Add(FString::Printf(TEXT("UMG widget '%s' does not have a valid cached Slate representation."), *WidgetPath));
			return Result;
		}
	}

	FSlateApplication& SlateApp = FSlateApplication::Get();
	FGeometry Geometry = TargetSlateWidget->GetTickSpaceGeometry();
	FVector2D Center = Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize() * 0.5f;

	TSharedPtr<SWindow> Window = SlateApp.GetActiveTopLevelWindow();
	TSharedPtr<FGenericWindow> GenWindow = Window.IsValid() ? Window->GetNativeWindow() : nullptr;

	TSet<FKey> PressedButtons;
	PressedButtons.Add(EKeys::LeftMouseButton);

	FPointerEvent MouseDownEvent(
		SlateApp.CursorPointerIndex,
		Center,
		Center,
		PressedButtons,
		EKeys::LeftMouseButton,
		0.0f,
		FModifierKeysState()
	);
	SlateApp.ProcessMouseButtonDownEvent(GenWindow, MouseDownEvent);

	TSet<FKey> EmptyPressedButtons;

	FPointerEvent MouseUpEvent(
		SlateApp.CursorPointerIndex,
		Center,
		Center,
		EmptyPressedButtons,
		EKeys::LeftMouseButton,
		0.0f,
		FModifierKeysState()
	);
	SlateApp.ProcessMouseButtonUpEvent(MouseUpEvent);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully programmatically clicked widget '%s' at screen coordinates (%f, %f)."), *WidgetPath, Center.X, Center.Y);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkPIEActions::ExecuteQueryWorldState(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!IsPIERunning())
	{
		Result.Errors.Add(TEXT("PIE is not running."));
		return Result;
	}

	UWorld* World = nullptr;
	if (GEditor)
	{
		FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
		if (PIEWorldContext)
		{
			World = PIEWorldContext->World();
		}
	}
	if (!World)
	{
		Result.Errors.Add(TEXT("Could not find active PIE world."));
		return Result;
	}

	TArray<AActor*> MatchingActors;
	
	TArray<UClass*> ClassFilters;
	const TArray<TSharedPtr<FJsonValue>>* ClassNamesJson;
	if (Params->TryGetArrayField(TEXT("classes"), ClassNamesJson))
	{
		for (const auto& Val : *ClassNamesJson)
		{
			FString ClassName = Val->AsString();
			UClass* TargetClass = UClass::TryFindTypeSlow<UClass>(*ClassName);
			if (!TargetClass)
			{
				TargetClass = FindObject<UClass>(nullptr, *ClassName);
			}
			if (TargetClass)
			{
				ClassFilters.Add(TargetClass);
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Could not find class type: '%s'"), *ClassName));
			}
		}
	}
	
	TArray<FString> TagFilters;
	const TArray<TSharedPtr<FJsonValue>>* TagsJson;
	if (Params->TryGetArrayField(TEXT("tags"), TagsJson))
	{
		for (const auto& Val : *TagsJson)
		{
			TagFilters.Add(Val->AsString());
		}
	}
	
	APlayerController* PlayerController = World->GetFirstPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	FVector PlayerLoc = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;

	float Radius = 0.0f;
	Params->TryGetNumberField(TEXT("radius"), Radius);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor)) continue;

		if (ClassFilters.Num() > 0)
		{
			bool bMatchesClass = false;
			for (UClass* ClassFilter : ClassFilters)
			{
				if (Actor->IsA(ClassFilter))
				{
					bMatchesClass = true;
					break;
				}
			}
			if (!bMatchesClass) continue;
		}

		if (TagFilters.Num() > 0)
		{
			bool bMatchesTag = false;
			for (const FString& TagFilter : TagFilters)
			{
				if (Actor->ActorHasTag(*TagFilter))
				{
					bMatchesTag = true;
					break;
				}
			}
			if (!bMatchesTag) continue;
		}

		if (Radius > 0.0f && PlayerPawn)
		{
			float Distance = FVector::Dist(PlayerLoc, Actor->GetActorLocation());
			if (Distance > Radius) continue;
		}

		MatchingActors.Add(Actor);
	}
	
	TSharedRef<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	
	if (PlayerPawn)
	{
		TSharedRef<FJsonObject> PlayerJson = MakeShared<FJsonObject>();
		PlayerJson->SetStringField(TEXT("class"), PlayerPawn->GetClass()->GetName());
		PlayerJson->SetStringField(TEXT("name"), PlayerPawn->GetName());
		
		TSharedRef<FJsonObject> LocJson = MakeShared<FJsonObject>();
		LocJson->SetNumberField(TEXT("x"), PlayerLoc.X);
		LocJson->SetNumberField(TEXT("y"), PlayerLoc.Y);
		LocJson->SetNumberField(TEXT("z"), PlayerLoc.Z);
		PlayerJson->SetObjectField(TEXT("location"), LocJson);
		
		ResponseObj->SetObjectField(TEXT("player"), PlayerJson);
	}

	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	for (AActor* Actor : MatchingActors)
	{
		if (Actor == PlayerPawn) continue;
		
		TSharedRef<FJsonObject> ActorJson = MakeShared<FJsonObject>();
		ActorJson->SetStringField(TEXT("name"), Actor->GetName());
		ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		
		FVector Loc = Actor->GetActorLocation();
		TSharedRef<FJsonObject> LocJson = MakeShared<FJsonObject>();
		LocJson->SetNumberField(TEXT("x"), Loc.X);
		LocJson->SetNumberField(TEXT("y"), Loc.Y);
		LocJson->SetNumberField(TEXT("z"), Loc.Z);
		ActorJson->SetObjectField(TEXT("location"), LocJson);
		
		TArray<TSharedPtr<FJsonValue>> ActorTags;
		for (const FName& Tag : Actor->Tags)
		{
			ActorTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
		}
		ActorJson->SetArrayField(TEXT("tags"), ActorTags);
		
		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
	}
	ResponseObj->SetArrayField(TEXT("actors"), ActorsArray);

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObj, Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseString;
	return Result;
}

#undef LOCTEXT_NAMESPACE
