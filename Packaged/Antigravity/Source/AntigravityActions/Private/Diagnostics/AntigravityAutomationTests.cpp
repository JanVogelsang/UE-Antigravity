// Copyright 2026 Antigravity. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Blueprint/AntigravityBlueprintActions.h"
#include "Widget/AntigravityWidgetActions.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ObjectTools.h"
#include "FileHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAntigravityTokenEfficiencyTest, "Antigravity.TokenEfficiency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAntigravityTokenEfficiencyTest::RunTest(const FString& Parameters)
{
	// ========================================================================
	// Test 1: Asset Path Expansion/Compression
	// ========================================================================
	{
		FAntigravityBlueprintActions BlueprintActions;
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("asset_path"), TEXT("TestFolder/TestActor"));
		Params->SetStringField(TEXT("_tool_name"), TEXT("get_blueprint_info"));
		TArray<FString> Errors;
		
		bool bValid = BlueprintActions.ValidateParams(Params, Errors);
		TestTrue(TEXT("ValidateParams should return true for valid relative path"), bValid);
		TestEqual(TEXT("Relative asset path should expand to start with /Game/"), 
			Params->GetStringField(TEXT("asset_path")), 
			TEXT("/Game/TestFolder/TestActor"));
	}

	{
		FAntigravityWidgetActions WidgetActions;
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("asset_path"), TEXT("TestFolder/TestWidget"));
		Params->SetStringField(TEXT("_tool_name"), TEXT("get_widget_tree"));
		TArray<FString> Errors;

		bool bValid = WidgetActions.ValidateParams(Params, Errors);
		TestTrue(TEXT("Widget ValidateParams should return true"), bValid);
		TestEqual(TEXT("Widget relative path should expand to /Game/"),
			Params->GetStringField(TEXT("asset_path")),
			TEXT("/Game/TestFolder/TestWidget"));
	}

	// ========================================================================
	// Setup Assets for Integration Testing
	// ========================================================================
	FString ActorPath = TEXT("/Game/Antigravity_TestActor");
	FString WidgetPath = TEXT("/Game/Antigravity_TestWidget");

	// Clean up any left-over assets from previous failed test runs
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData ExistingActorAsset = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ActorPath + TEXT(".") + FPackageName::GetShortName(ActorPath)));
	if (ExistingActorAsset.IsValid())
	{
		UObject* Obj = ExistingActorAsset.GetAsset();
		if (Obj)
		{
			TArray<UObject*> AssetsToDelete;
			AssetsToDelete.Add(Obj);
			ObjectTools::DeleteObjects(AssetsToDelete, false);
		}
	}
	FAssetData ExistingWidgetAsset = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(WidgetPath + TEXT(".") + FPackageName::GetShortName(WidgetPath)));
	if (ExistingWidgetAsset.IsValid())
	{
		UObject* Obj = ExistingWidgetAsset.GetAsset();
		if (Obj)
		{
			TArray<UObject*> AssetsToDelete;
			AssetsToDelete.Add(Obj);
			ObjectTools::DeleteObjects(AssetsToDelete, false);
		}
	}

	FAntigravityBlueprintActions BlueprintActions;
	FAntigravityWidgetActions WidgetActions;

	// Create test blueprint actor
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("_tool_name"), TEXT("create_blueprint_actor"));
		Params->SetStringField(TEXT("asset_path"), ActorPath);
		Params->SetStringField(TEXT("parent_class"), TEXT("Actor"));

		FAntigravityActionResult Res = BlueprintActions.ExecuteAction(Params);
		TestTrue(TEXT("Should successfully create a blueprint actor"), Res.bSuccess);
	}

	// Create test widget blueprint
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("_tool_name"), TEXT("create_widget_blueprint"));
		Params->SetStringField(TEXT("asset_path"), WidgetPath);

		FAntigravityActionResult Res = WidgetActions.ExecuteAction(Params);
		TestTrue(TEXT("Should successfully create a widget blueprint"), Res.bSuccess);
	}

	// ========================================================================
	// Test 2: Visual Layout coordinate exclusion and query mode
	// ========================================================================
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("_tool_name"), TEXT("get_blueprint_info"));
		Params->SetStringField(TEXT("asset_path"), ActorPath);
		Params->SetBoolField(TEXT("exclude_visual_layout"), true);

		FAntigravityActionResult Res = BlueprintActions.ExecuteAction(Params);
		TestTrue(TEXT("get_blueprint_info with exclude_visual_layout = true should succeed"), Res.bSuccess);

		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res.ResultMessage);
		TestTrue(TEXT("Response should be valid JSON"), FJsonSerializer::Deserialize(Reader, JsonObj));
		TestTrue(TEXT("Response JSON should be valid"), JsonObj.IsValid());

		// Verify no visual coordinate fields in the nodes array
		const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
		if (JsonObj->TryGetArrayField(TEXT("nodes"), NodesArray))
		{
			for (const TSharedPtr<FJsonValue>& NodeVal : *NodesArray)
			{
				TSharedPtr<FJsonObject> NodeObj = NodeVal->AsObject();
				if (NodeObj.IsValid())
				{
					TestFalse(TEXT("Nodes array should not contain pos_x when visual layout is excluded"), NodeObj->HasField(TEXT("pos_x")));
					TestFalse(TEXT("Nodes array should not contain pos_y when visual layout is excluded"), NodeObj->HasField(TEXT("pos_y")));
				}
			}
		}
	}

	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("_tool_name"), TEXT("get_blueprint_info"));
		Params->SetStringField(TEXT("asset_path"), ActorPath);
		Params->SetStringField(TEXT("query_mode"), TEXT("interface_only"));

		FAntigravityActionResult Res = BlueprintActions.ExecuteAction(Params);
		TestTrue(TEXT("get_blueprint_info with query_mode = interface_only should succeed"), Res.bSuccess);

		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res.ResultMessage);
		TestTrue(TEXT("Response should be valid JSON"), FJsonSerializer::Deserialize(Reader, JsonObj));
		TestTrue(TEXT("Response JSON should be valid"), JsonObj.IsValid());

		const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
		if (JsonObj->TryGetArrayField(TEXT("nodes"), NodesArray))
		{
			for (const TSharedPtr<FJsonValue>& NodeVal : *NodesArray)
			{
				TSharedPtr<FJsonObject> NodeObj = NodeVal->AsObject();
				if (NodeObj.IsValid())
				{
					FString NodeType;
					NodeObj->TryGetStringField(TEXT("type"), NodeType);
					// Interface mode should only have Entry/Result/Event/Tunnel nodes, not raw operators/variables/etc.
					TestTrue(TEXT("Node type in interface_only should be limited to interface nodes"),
						NodeType.IsEmpty() ||
						NodeType.Contains(TEXT("Entry")) ||
						NodeType.Contains(TEXT("Result")) ||
						NodeType.Contains(TEXT("Event")) ||
						NodeType.Contains(TEXT("Tunnel")));
				}
			}
		}
	}

	// ========================================================================
	// Test 3: Client hash up-to-date checking
	// ========================================================================
	{
		FString ClientHash;
		{
			TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
			Params->SetStringField(TEXT("_tool_name"), TEXT("get_blueprint_info"));
			Params->SetStringField(TEXT("asset_path"), ActorPath);

			FAntigravityActionResult Res = BlueprintActions.ExecuteAction(Params);
			TestTrue(TEXT("get_blueprint_info should succeed"), Res.bSuccess);

			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res.ResultMessage);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				JsonObj->TryGetStringField(TEXT("client_hash"), ClientHash);
			}
		}

		TestFalse(TEXT("ClientHash should not be empty"), ClientHash.IsEmpty());

		// Query again with client_hash set to the same value
		{
			TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
			Params->SetStringField(TEXT("_tool_name"), TEXT("get_blueprint_info"));
			Params->SetStringField(TEXT("asset_path"), ActorPath);
			Params->SetStringField(TEXT("client_hash"), ClientHash);

			FAntigravityActionResult Res = BlueprintActions.ExecuteAction(Params);
			TestTrue(TEXT("get_blueprint_info with matching client_hash should succeed"), Res.bSuccess);

			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res.ResultMessage);
			TestTrue(TEXT("Response should deserialize"), FJsonSerializer::Deserialize(Reader, JsonObj));
			TestTrue(TEXT("Response should indicate it is up to date"), JsonObj->GetBoolField(TEXT("up_to_date")));
		}
	}

	// ========================================================================
	// Test 4: UI Hierarchy instantiation (instantiate_ui_hierarchy)
	// ========================================================================
	{
		// Construct instantiate_ui_hierarchy payload
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("_tool_name"), TEXT("instantiate_ui_hierarchy"));
		Params->SetStringField(TEXT("asset_path"), WidgetPath);

		TArray<TSharedPtr<FJsonValue>> WidgetsArray;
		
		// 1. Root CanvasPanel
		TSharedRef<FJsonObject> CanvasObj = MakeShared<FJsonObject>();
		CanvasObj->SetStringField(TEXT("widget_class"), TEXT("CanvasPanel"));
		CanvasObj->SetStringField(TEXT("widget_name"), TEXT("MyRootCanvas"));
		WidgetsArray.Add(MakeShared<FJsonValueObject>(CanvasObj));

		// 2. Button child of CanvasPanel
		TSharedRef<FJsonObject> ButtonObj = MakeShared<FJsonObject>();
		ButtonObj->SetStringField(TEXT("widget_class"), TEXT("Button"));
		ButtonObj->SetStringField(TEXT("widget_name"), TEXT("SubmitButton"));
		ButtonObj->SetStringField(TEXT("parent_widget"), TEXT("MyRootCanvas"));
		
		// Slot settings for Button
		TSharedRef<FJsonObject> ButtonSlotObj = MakeShared<FJsonObject>();
		ButtonSlotObj->SetStringField(TEXT("anchors_min"), TEXT("0.5,0.5"));
		ButtonSlotObj->SetStringField(TEXT("anchors_max"), TEXT("0.5,0.5"));
		ButtonSlotObj->SetStringField(TEXT("offsets"), TEXT("-100,-40,100,40"));
		ButtonObj->SetObjectField(TEXT("slot"), ButtonSlotObj);

		// Brush settings for Button
		TSharedRef<FJsonObject> ButtonBrushObj = MakeShared<FJsonObject>();
		ButtonBrushObj->SetStringField(TEXT("brush_target"), TEXT("Normal"));
		ButtonBrushObj->SetStringField(TEXT("tint_color"), TEXT("(R=0,G=0,B=1,A=1)"));
		ButtonObj->SetObjectField(TEXT("brush"), ButtonBrushObj);

		// Event settings for Button
		TSharedRef<FJsonObject> ButtonEventsObj = MakeShared<FJsonObject>();
		ButtonEventsObj->SetStringField(TEXT("OnClicked"), TEXT("OnSubmitButtonClicked"));
		ButtonObj->SetObjectField(TEXT("events"), ButtonEventsObj);

		WidgetsArray.Add(MakeShared<FJsonValueObject>(ButtonObj));

		// 3. TextBlock child of Button
		TSharedRef<FJsonObject> TextObj = MakeShared<FJsonObject>();
		TextObj->SetStringField(TEXT("widget_class"), TEXT("TextBlock"));
		TextObj->SetStringField(TEXT("widget_name"), TEXT("ButtonText"));
		TextObj->SetStringField(TEXT("parent_widget"), TEXT("SubmitButton"));

		// Properties for TextBlock
		TSharedRef<FJsonObject> TextPropertiesObj = MakeShared<FJsonObject>();
		TextPropertiesObj->SetStringField(TEXT("Text"), TEXT("Submit"));
		TextObj->SetObjectField(TEXT("properties"), TextPropertiesObj);

		// Font settings for TextBlock
		TSharedRef<FJsonObject> TextFontObj = MakeShared<FJsonObject>();
		TextFontObj->SetNumberField(TEXT("font_size"), 24);
		TextFontObj->SetStringField(TEXT("color"), TEXT("(R=1,G=1,B=1,A=1)"));
		TextObj->SetObjectField(TEXT("font"), TextFontObj);

		WidgetsArray.Add(MakeShared<FJsonValueObject>(TextObj));

		Params->SetArrayField(TEXT("widgets"), WidgetsArray);

		FAntigravityActionResult Res = WidgetActions.ExecuteAction(Params);
		TestTrue(TEXT("instantiate_ui_hierarchy should execute successfully"), Res.bSuccess);

		// Verify the constructed UI hierarchy in the Widget Blueprint asset
		UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(ExistingWidgetAsset.GetAsset());
		if (WidgetBP)
		{
			WidgetBP->PostLoad(); // Ensure it is fully loaded/constructed
			UWidgetTree* Tree = WidgetBP->WidgetTree;
			TestTrue(TEXT("WidgetTree should exist"), Tree != nullptr);
			if (Tree)
			{
				UWidget* CanvasWidget = Tree->FindWidget(FName("MyRootCanvas"));
				TestTrue(TEXT("Root CanvasPanel widget should exist"), CanvasWidget != nullptr);

				UWidget* ButtonWidget = Tree->FindWidget(FName("SubmitButton"));
				TestTrue(TEXT("SubmitButton widget should exist"), ButtonWidget != nullptr);
				if (ButtonWidget)
				{
					TestEqual(TEXT("Button parent should be CanvasPanel"), Cast<UWidget>(ButtonWidget->GetParent()), CanvasWidget);
					
					// Verify slot layout
					UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ButtonWidget->Slot);
					TestTrue(TEXT("Button slot should be a CanvasPanelSlot"), CanvasSlot != nullptr);
					if (CanvasSlot)
					{
						TestEqual(TEXT("Slot Minimum Anchor should be 0.5"), CanvasSlot->GetAnchors().Minimum.X, 0.5);
						TestEqual(TEXT("Slot Maximum Anchor should be 0.5"), CanvasSlot->GetAnchors().Maximum.Y, 0.5);
					}
				}

				UWidget* TextWidget = Tree->FindWidget(FName("ButtonText"));
				TestTrue(TEXT("ButtonText widget should exist"), TextWidget != nullptr);
				if (TextWidget && ButtonWidget)
				{
					TestEqual(TEXT("TextBlock parent should be Button"), Cast<UWidget>(TextWidget->GetParent()), ButtonWidget);
					UTextBlock* TextBlock = Cast<UTextBlock>(TextWidget);
					TestTrue(TEXT("TextWidget should be a UTextBlock instance"), TextBlock != nullptr);
					if (TextBlock)
					{
						TestEqual(TEXT("TextBlock text should be set to Submit"), TextBlock->GetText().ToString(), TEXT("Submit"));
						TestEqual(TEXT("TextBlock font size should be 24"), TextBlock->GetFont().Size, 24.0f);
					}
				}
			}
		}
	}

	// ========================================================================
	// Cleanup Assets
	// ========================================================================
	{
		TArray<UObject*> AssetsToDelete;
		FAssetData ActorAsset = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ActorPath + TEXT(".") + FPackageName::GetShortName(ActorPath)));
		if (ActorAsset.IsValid() && ActorAsset.GetAsset())
		{
			AssetsToDelete.Add(ActorAsset.GetAsset());
		}
		FAssetData WidgetAsset = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(WidgetPath + TEXT(".") + FPackageName::GetShortName(WidgetPath)));
		if (WidgetAsset.IsValid() && WidgetAsset.GetAsset())
		{
			AssetsToDelete.Add(WidgetAsset.GetAsset());
		}

		if (AssetsToDelete.Num() > 0)
		{
			ObjectTools::DeleteObjects(AssetsToDelete, false);
		}
	}

	return true;
}
