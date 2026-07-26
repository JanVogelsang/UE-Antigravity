// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "AgentFrameworkActionRouter.h"
#include "Blueprint/AgentFrameworkBlueprintActions.h"
#include "Cpp/AgentFrameworkCppActions.h"
#include "Widget/AgentFrameworkWidgetActions.h"
#include "Diagnostics/AgentFrameworkDiagnosticsActions.h"
#include "Dom/JsonObject.h"
#include "AgentFrameworkActionUtils.h"
#include "AIAssistant/AIAssistantBridge.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentFrameworkTokenEfficiencyTest, "AgentFramework.TokenEfficiency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAgentFrameworkTokenEfficiencyTest::RunTest(const FString& Parameters)
{
	// ========================================================================
	// Test 1: Asset Path Expansion/Compression
	// ========================================================================
	{
		FAgentFrameworkBlueprintActions BlueprintActions;
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
		FAgentFrameworkWidgetActions WidgetActions;
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
	FString ActorPath = TEXT("/Game/AgentFramework_TestActor");
	FString WidgetPath = TEXT("/Game/AgentFramework_TestWidget");

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

	FAgentFrameworkBlueprintActions BlueprintActions;
	FAgentFrameworkWidgetActions WidgetActions;

	// Create test blueprint actor
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("_tool_name"), TEXT("create_blueprint_actor"));
		Params->SetStringField(TEXT("asset_path"), ActorPath);
		Params->SetStringField(TEXT("parent_class"), TEXT("Actor"));

		FAgentFrameworkActionResult Res = BlueprintActions.ExecuteAction(Params);
		TestTrue(TEXT("Should successfully create a blueprint actor"), Res.bSuccess);
	}

	// Create test widget blueprint
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("_tool_name"), TEXT("create_widget_blueprint"));
		Params->SetStringField(TEXT("asset_path"), WidgetPath);

		FAgentFrameworkActionResult Res = WidgetActions.ExecuteAction(Params);
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

		FAgentFrameworkActionResult Res = BlueprintActions.ExecuteAction(Params);
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

		FAgentFrameworkActionResult Res = BlueprintActions.ExecuteAction(Params);
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

			FAgentFrameworkActionResult Res = BlueprintActions.ExecuteAction(Params);
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

			FAgentFrameworkActionResult Res = BlueprintActions.ExecuteAction(Params);
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

		FAgentFrameworkActionResult Res = WidgetActions.ExecuteAction(Params);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentFrameworkGuidResolutionTest, "AgentFramework.GuidResolution", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAgentFrameworkGuidResolutionTest::RunTest(const FString& Parameters)
{
	// Test input with overlapping prefix placeholders
	FString MockT3D = TEXT("NodeGuid=GUID_Node1\n"
						   "PinId=LINK_1\n"
						   "NodeGuid=GUID_Node10\n"
						   "PinId=LINK_10\n"
						   "LinkedTo=(GUID_Node1 LINK_1)\n"
						   "LinkedTo=(GUID_Node10 LINK_10)");

	// Access FAgentFrameworkBlueprintActions to resolve placeholders
	struct FTestBlueprintActions : public FAgentFrameworkBlueprintActions
	{
		using FAgentFrameworkBlueprintActions::ResolveT3DPlaceholders;
	};

	FString Resolved = FTestBlueprintActions::ResolveT3DPlaceholders(MockT3D);

	// Check that placeholders have been replaced by 32-character hex GUIDs
	// And that LINK_1 replacement didn't corrupt LINK_10
	TestFalse(TEXT("Should resolve GUID_Node1"), Resolved.Contains(TEXT("GUID_Node1")));
	TestFalse(TEXT("Should resolve GUID_Node10"), Resolved.Contains(TEXT("GUID_Node10")));
	TestFalse(TEXT("Should resolve LINK_1"), Resolved.Contains(TEXT("LINK_1")));
	TestFalse(TEXT("Should resolve LINK_10"), Resolved.Contains(TEXT("LINK_10")));

	// Verify total length or formatting patterns to ensure correct replacement
	TArray<FString> Lines;
	Resolved.ParseIntoArrayLines(Lines);
	
	FString LineNode1 = Lines[0];  // NodeGuid=[GUID]
	FString LineLink1 = Lines[1];  // PinId=[GUID]
	FString LineNode10 = Lines[2]; // NodeGuid=[GUID]
	FString LineLink10 = Lines[3]; // PinId=[GUID]
	
	FString Guid1Val = LineNode1.Mid(LineNode1.Find(TEXT("=")) + 1);
	FString Link1Val = LineLink1.Mid(LineLink1.Find(TEXT("=")) + 1);
	FString Guid10Val = LineNode10.Mid(LineNode10.Find(TEXT("=")) + 1);
	FString Link10Val = LineLink10.Mid(LineLink10.Find(TEXT("=")) + 1);

	TestEqual(TEXT("GUID_Node1 replacement length should be 32"), Guid1Val.Len(), 32);
	TestEqual(TEXT("LINK_1 replacement length should be 32"), Link1Val.Len(), 32);
	TestEqual(TEXT("GUID_Node10 replacement length should be 32"), Guid10Val.Len(), 32);
	TestEqual(TEXT("LINK_10 replacement length should be 32"), Link10Val.Len(), 32);
	TestNotEqual(TEXT("GUID_Node1 and GUID_Node10 must be different"), Guid1Val, Guid10Val);
	TestNotEqual(TEXT("LINK_1 and LINK_10 must be different"), Link1Val, Link10Val);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentFrameworkCppReflectionTest, "AgentFramework.CppReflection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAgentFrameworkCppReflectionTest::RunTest(const FString& Parameters)
{
	FAgentFrameworkCppActions CppActions;
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("_tool_name"), TEXT("get_cpp_reflection_info"));
	Params->SetStringField(TEXT("class_name"), TEXT("Actor")); // Test against engine Actor class
	
	FAgentFrameworkActionResult Result = CppActions.ExecuteAction(Params);
	TestTrue(TEXT("Reflection lookup should succeed for Actor"), Result.bSuccess);

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Result.ResultMessage);
	TestTrue(TEXT("Should parse JSON output"), FJsonSerializer::Deserialize(Reader, JsonObj));
	TestTrue(TEXT("JSON should be valid"), JsonObj.IsValid());

	TestEqual(TEXT("Class name should match"), JsonObj->GetStringField(TEXT("class_name")), TEXT("Actor"));
	TestEqual(TEXT("Parent class should match"), JsonObj->GetStringField(TEXT("parent_class")), TEXT("Object"));
	
	// Verify properties and functions arrays exist
	TestTrue(TEXT("Should have properties"), JsonObj->HasField(TEXT("properties")));
	TestTrue(TEXT("Should have functions"), JsonObj->HasField(TEXT("functions")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentFrameworkSentinelTest, "AgentFramework.Sentinel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAgentFrameworkSentinelTest::RunTest(const FString& Parameters)
{
	FString ActorPath = TEXT("/Game/AgentFramework_SentinelTestActor");

	// Clean up if existing
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData ExistingActorAsset = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ActorPath + TEXT(".") + FPackageName::GetShortName(ActorPath)));
	if (ExistingActorAsset.IsValid() && ExistingActorAsset.GetAsset())
	{
		TArray<UObject*> AssetsToDelete;
		AssetsToDelete.Add(ExistingActorAsset.GetAsset());
		ObjectTools::DeleteObjects(AssetsToDelete, false);
	}

	FAgentFrameworkBlueprintActions BlueprintActions;

	// 1. Create a clean Blueprint
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(TEXT("_tool_name"), TEXT("create_blueprint_actor"));
		Params->SetStringField(TEXT("asset_path"), ActorPath);
		Params->SetStringField(TEXT("parent_class"), TEXT("Actor"));

		FAgentFrameworkActionResult Res = BlueprintActions.ExecuteAction(Params);
		TestTrue(TEXT("Should create a clean blueprint actor"), Res.bSuccess);
	}

	// Make sure it exists and load it
	UPackage* Package = FindPackage(nullptr, *ActorPath);
	TestNotNull(TEXT("Package should exist"), Package);

	if (Package)
	{
		// 2. Mark package dirty (simulate user edit)
		Package->MarkPackageDirty();
		TestTrue(TEXT("Package should be dirty"), Package->IsDirty());

		// 3. Try to run a modifying action (e.g. add a variable)
		{
			TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
			Params->SetStringField(TEXT("_tool_name"), TEXT("add_blueprint_variable"));
			Params->SetStringField(TEXT("asset_path"), ActorPath);
			Params->SetStringField(TEXT("variable_name"), TEXT("TestSentinelVar"));
			Params->SetStringField(TEXT("variable_type"), TEXT("float"));

			FAgentFrameworkActionResult Res = BlueprintActions.ExecuteAction(Params);
			TestFalse(TEXT("Sentinel should block modifications when package is dirty"), Res.bSuccess);
			TestTrue(TEXT("Result should contain SENTINEL ERROR warning/error"), Res.Errors.Num() > 0 && Res.Errors[0].Contains(TEXT("SENTINEL ERROR")));
		}

		// 4. Test check_asset_state tool
		{
			TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
			Params->SetStringField(TEXT("_tool_name"), TEXT("check_asset_state"));
			Params->SetStringField(TEXT("asset_path"), ActorPath);

			FAgentFrameworkActionResult Res = BlueprintActions.ExecuteAction(Params);
			TestTrue(TEXT("check_asset_state should execute successfully"), Res.bSuccess);

			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res.ResultMessage);
			TestTrue(TEXT("check_asset_state output should be JSON"), FJsonSerializer::Deserialize(Reader, JsonObj));
			if (JsonObj.IsValid())
			{
				TestTrue(TEXT("bIsDirty should be true"), JsonObj->GetBoolField(TEXT("bIsDirty")));
			}
		}

		// 5. Clean up the package (un-dirty it and delete)
		Package->SetDirtyFlag(false);
	}

	// Clean up asset
	FAssetData ActorAsset = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ActorPath + TEXT(".") + FPackageName::GetShortName(ActorPath)));
	if (ActorAsset.IsValid() && ActorAsset.GetAsset())
	{
		TArray<UObject*> AssetsToDelete;
		AssetsToDelete.Add(ActorAsset.GetAsset());
		ObjectTools::DeleteObjects(AssetsToDelete, false);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentFrameworkAIAssistantTests, "AgentFramework.AIAssistant", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAgentFrameworkAIAssistantTests::RunTest(const FString& Parameters)
{
	// Test UAgentFrameworkActionUtils
	{
		TSharedPtr<FJsonObject> TestParams = MakeShared<FJsonObject>();
		TestParams->SetStringField(TEXT("test_str"), TEXT("HelloValue"));
		TestParams->SetBoolField(TEXT("test_bool"), true);
		TestParams->SetNumberField(TEXT("test_double"), 123.45);

		FString OutStr;
		bool OutBool = false;
		double OutDouble = 0.0;
		TArray<FString> Errors;

		// Test success cases
		bool bStrOk = UAgentFrameworkActionUtils::TryGetStringParam(TestParams, TEXT("test_str"), OutStr, Errors, true);
		TestTrue(TEXT("TryGetStringParam should succeed"), bStrOk);
		TestEqual(TEXT("TryGetStringParam value matches"), OutStr, TEXT("HelloValue"));

		bool bBoolOk = UAgentFrameworkActionUtils::TryGetBoolParam(TestParams, TEXT("test_bool"), OutBool, Errors, true);
		TestTrue(TEXT("TryGetBoolParam should succeed"), bBoolOk);
		TestTrue(TEXT("TryGetBoolParam value matches"), OutBool);

		bool bDoubleOk = UAgentFrameworkActionUtils::TryGetDoubleParam(TestParams, TEXT("test_double"), OutDouble, Errors, true);
		TestTrue(TEXT("TryGetDoubleParam should succeed"), bDoubleOk);
		TestEqual(TEXT("TryGetDoubleParam value matches"), OutDouble, 123.45);

		// Test failure case: missing required parameter
		TArray<FString> ValidationErrors;
		FString MissingStr;
		bool bMissingOk = UAgentFrameworkActionUtils::TryGetStringParam(TestParams, TEXT("missing_str"), MissingStr, ValidationErrors, true);
		TestFalse(TEXT("TryGetStringParam should fail for missing required parameter"), bMissingOk);
		TestTrue(TEXT("ValidationErrors should contain error message"), ValidationErrors.Num() > 0);
	}

	// Test UAIAssistantBridge Multicast Delegate Hook
	{
		UAIAssistantBridge* TestBridge = NewObject<UAIAssistantBridge>();
		TestNotNull(TEXT("Should create UAIAssistantBridge"), TestBridge);
		if (IsValid(TestBridge))
		{
			bool bDelegateFired = false;
			FString ReceivedResponse;
			bool bReceivedSuccess = false;

			TestBridge->OnQueryCompleted.AddLambda([&bDelegateFired, &ReceivedResponse, &bReceivedSuccess](const FString& Response, bool bSuccess)
			{
				bDelegateFired = true;
				ReceivedResponse = Response;
				bReceivedSuccess = bSuccess;
			});

			TestBridge->OnResponseReceived(TEXT("Test AI Response"), true);

			TestTrue(TEXT("OnQueryCompleted delegate should fire when response received"), bDelegateFired);
			TestEqual(TEXT("Response message should match"), ReceivedResponse, TEXT("Test AI Response"));
			TestTrue(TEXT("Success boolean should match"), bReceivedSuccess);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentFrameworkDiagnosticsActionsTest, "AgentFramework.DiagnosticsActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAgentFrameworkDiagnosticsActionsTest::RunTest(const FString& Parameters)
{
	FAgentFrameworkDiagnosticsActions DiagnosticsActions;
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("_tool_name"), TEXT("read_message_log"));
	Params->SetNumberField(TEXT("max_lines"), 10);

	FAgentFrameworkActionResult Result = DiagnosticsActions.ExecuteAction(Params);
	TestTrue(TEXT("read_message_log should succeed"), Result.bSuccess);
	TestTrue(TEXT("Result message should contain Output Log header"), Result.ResultMessage.Contains(TEXT("=== Output Log ===")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentFrameworkAsyncRouterTest, "AgentFramework.AsyncRouter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAgentFrameworkAsyncRouterTest::RunTest(const FString& Parameters)
{
	TSharedRef<FAgentFrameworkActionRouter> Router = MakeShared<FAgentFrameworkActionRouter>();
	Router->RegisterExecutor(MakeShared<FAgentFrameworkDiagnosticsActions>());

	FAgentFrameworkToolCall ToolCall;
	ToolCall.ToolCallId = FGuid::NewGuid().ToString();
	ToolCall.ToolName = TEXT("read_message_log");
	ToolCall.InputParams = MakeShared<FJsonObject>();
	ToolCall.InputParams->SetNumberField(TEXT("max_lines"), 5);

	bool bCallbackFired = false;
	FAgentFrameworkActionResult AsyncResult;

	FGuid TaskId = Router->RouteToolCallAsync(ToolCall, [&bCallbackFired, &AsyncResult](FAgentFrameworkActionResult Res) {
		bCallbackFired = true;
		AsyncResult = Res;
	});

	TestTrue(TEXT("TaskId should be valid"), TaskId.IsValid());

	// Test cancellation functionality
	FAgentFrameworkToolCall CancelToolCall;
	CancelToolCall.ToolCallId = FGuid::NewGuid().ToString();
	CancelToolCall.ToolName = TEXT("read_message_log");
	CancelToolCall.InputParams = MakeShared<FJsonObject>();

	bool bCancelledCallbackFired = false;
	FGuid CancelTaskId = Router->RouteToolCallAsync(CancelToolCall, [this, &bCancelledCallbackFired](FAgentFrameworkActionResult Res) {
		bCancelledCallbackFired = true;
		TestFalse(TEXT("Cancelled task result should be false"), Res.bSuccess);
	});

	bool bCancelled = Router->CancelTask(CancelTaskId);
	TestTrue(TEXT("CancelTask should return true for pending task"), bCancelled);
	TestTrue(TEXT("Cancelled task callback should be invoked"), bCancelledCallbackFired);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAgentFrameworkTelemetryTest, "AgentFramework.Telemetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAgentFrameworkTelemetryTest::RunTest(const FString& Parameters)
{
	// 1. Reset telemetry data to ensure clean state
	UAgentFrameworkActionUtils::ClearTelemetryData();

	// Verify initially empty
	TArray<FAgentFrameworkToolMetrics> InitialMetrics = UAgentFrameworkActionUtils::GetToolTelemetry();
	TestEqual(TEXT("Initial metrics should be empty after clear"), InitialMetrics.Num(), 0);

	TArray<FAgentFrameworkErrorRecord> InitialErrors = UAgentFrameworkActionUtils::GetRecentErrors();
	TestEqual(TEXT("Initial errors should be empty after clear"), InitialErrors.Num(), 0);

	// 2. Simulate tool executions via ScopedTelemetry and direct RecordToolExecution
	FString ToolA = TEXT("test_tool_alpha");
	FString ToolB = TEXT("test_tool_beta");

	// Record success execution for ToolA using ScopedTelemetry
	{
		FAgentFrameworkScopedTelemetry Scoped(ToolA);
		FPlatformProcess::Sleep(0.002f); // Sleep ~2 milliseconds
		TArray<FString> NoErrors;
		Scoped.SetResult(true, NoErrors);
	}

	// Verify ToolA telemetry record
	TArray<FAgentFrameworkToolMetrics> MetricsA = UAgentFrameworkActionUtils::GetToolTelemetry(ToolA);
	TestEqual(TEXT("Should have 1 metrics record for ToolA"), MetricsA.Num(), 1);
	if (MetricsA.Num() > 0)
	{
		TestEqual(TEXT("ToolA total executions should be 1"), MetricsA[0].TotalExecutions, (int64)1);
		TestEqual(TEXT("ToolA success count should be 1"), MetricsA[0].SuccessCount, (int64)1);
		TestEqual(TEXT("ToolA error count should be 0"), MetricsA[0].ErrorCount, (int64)0);
		TestTrue(TEXT("ToolA duration should be > 500 microseconds"), MetricsA[0].TotalDurationMicros >= 500.0);
		TestTrue(TEXT("ToolA last success should be true"), MetricsA[0].bLastSuccess);
	}

	// Record failing executions for ToolB
	TArray<FString> ErrorsB;
	ErrorsB.Add(TEXT("Failed to connect pin"));
	ErrorsB.Add(TEXT("Invalid property target"));

	UAgentFrameworkActionUtils::RecordToolExecution(ToolB, 500.0, false, ErrorsB, TEXT("Context info"));

	// Record second identical error for ToolB to verify frequency deduplication
	UAgentFrameworkActionUtils::RecordToolExecution(ToolB, 600.0, false, ErrorsB, TEXT("Context info 2"));

	// Verify ToolB metrics
	TArray<FAgentFrameworkToolMetrics> MetricsB = UAgentFrameworkActionUtils::GetToolTelemetry(ToolB);
	TestEqual(TEXT("Should have 1 metrics record for ToolB"), MetricsB.Num(), 1);
	if (MetricsB.Num() > 0)
	{
		TestEqual(TEXT("ToolB total executions should be 2"), MetricsB[0].TotalExecutions, (int64)2);
		TestEqual(TEXT("ToolB success count should be 0"), MetricsB[0].SuccessCount, (int64)0);
		TestEqual(TEXT("ToolB error count should be 2"), MetricsB[0].ErrorCount, (int64)2);
		TestEqual(TEXT("ToolB min duration should be 500"), MetricsB[0].MinDurationMicros, 500.0);
		TestEqual(TEXT("ToolB max duration should be 600"), MetricsB[0].MaxDurationMicros, 600.0);
		TestEqual(TEXT("ToolB avg duration should be 550"), MetricsB[0].AvgDurationMicros, 550.0);
		TestFalse(TEXT("ToolB last success should be false"), MetricsB[0].bLastSuccess);
	}

	// Verify Error Ring Buffer memory
	TArray<FAgentFrameworkErrorRecord> RecentErrors = UAgentFrameworkActionUtils::GetRecentErrors(50, ToolB);
	TestTrue(TEXT("Should have recorded errors for ToolB"), RecentErrors.Num() > 0);

	// Check frequency increment for repeated error
	bool bFoundFrequencyCheck = false;
	for (const FAgentFrameworkErrorRecord& ErrRec : RecentErrors)
	{
		if (ErrRec.ToolName == ToolB && ErrRec.ErrorMessage == TEXT("Failed to connect pin"))
		{
			TestEqual(TEXT("Error frequency for repeated error should be 2"), ErrRec.Frequency, 2);
			bFoundFrequencyCheck = true;
		}
	}
	TestTrue(TEXT("Found frequency check for repeated error"), bFoundFrequencyCheck);

	// Test GetTelemetryMetricsJson
	FString JsonSummary = UAgentFrameworkActionUtils::GetTelemetryMetricsJson();
	TestTrue(TEXT("JSON summary should contain test_tool_alpha"), JsonSummary.Contains(ToolA));
	TestTrue(TEXT("JSON summary should contain test_tool_beta"), JsonSummary.Contains(ToolB));

	// Test ActionRouter integration with automatic telemetry
	{
		TSharedRef<FAgentFrameworkActionRouter> Router = MakeShared<FAgentFrameworkActionRouter>();
		Router->RegisterExecutor(MakeShared<FAgentFrameworkDiagnosticsActions>());

		FAgentFrameworkToolCall ToolCall;
		ToolCall.ToolCallId = FGuid::NewGuid().ToString();
		ToolCall.ToolName = TEXT("read_message_log");
		ToolCall.InputParams = MakeShared<FJsonObject>();
		ToolCall.InputParams->SetNumberField(TEXT("max_lines"), 5);

		FAgentFrameworkActionResult Result = Router->RouteToolCall(ToolCall);
		TestTrue(TEXT("Routed tool call should succeed"), Result.bSuccess);

		TArray<FAgentFrameworkToolMetrics> RouterMetrics = UAgentFrameworkActionUtils::GetToolTelemetry(TEXT("read_message_log"));
		TestTrue(TEXT("Router tool call should automatically record telemetry for read_message_log"), RouterMetrics.Num() > 0);
		if (RouterMetrics.Num() > 0)
		{
			TestTrue(TEXT("read_message_log execution count should be at least 1"), RouterMetrics[0].TotalExecutions >= 1);
		}
	}

	// Clean up at the end
	UAgentFrameworkActionUtils::ClearTelemetryData();
	return true;
}



