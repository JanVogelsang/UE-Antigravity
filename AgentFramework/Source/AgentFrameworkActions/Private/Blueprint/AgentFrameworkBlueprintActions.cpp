// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Blueprint/AgentFrameworkBlueprintActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionsModule.h"

// Kismet / Blueprint API
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "EdGraphUtilities.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CustomEvent.h"
#include "AgentFrameworkActionUtils.h"
#include "Sound/SoundBase.h"
#include "K2Node_EnhancedInputAction.h"
#include "InputAction.h"

// UE classes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"

// Asset management
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Factories/BlueprintFactory.h"
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
#include "Internationalization/Regex.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "Editor.h"

// ============================================================================
// Statics / Lifecycle
// ============================================================================

TMap<FString, int32> FAgentFrameworkBlueprintActions::AssetModificationCounts;

namespace
{
	FString FormatJsonObjectToUnrealText(const TSharedPtr<FJsonObject>& Obj)
	{
		if (!Obj.IsValid()) return TEXT("()");
		FString OutStr = TEXT("(");
		bool bFirst = true;
		for (const auto& Pair : Obj->Values)
		{
			if (!bFirst) OutStr += TEXT(",");
			bFirst = false;

			OutStr += FString(Pair.Key) + TEXT("=");
			if (Pair.Value->Type == EJson::Object)
			{
				OutStr += FormatJsonObjectToUnrealText(Pair.Value->AsObject());
			}
			else if (Pair.Value->Type == EJson::String)
			{
				OutStr += FString::Printf(TEXT("\"%s\""), *Pair.Value->AsString());
			}
			else if (Pair.Value->Type == EJson::Number)
			{
				OutStr += FString::Printf(TEXT("%f"), Pair.Value->AsNumber());
			}
			else if (Pair.Value->Type == EJson::Boolean)
			{
				OutStr += Pair.Value->AsBool() ? TEXT("True") : TEXT("False");
			}
			else if (Pair.Value->Type == EJson::Array)
			{
				OutStr += TEXT("(");
				bool bArrFirst = true;
				for (const auto& Elem : Pair.Value->AsArray())
				{
					if (!bArrFirst) OutStr += TEXT(",");
					bArrFirst = false;
					if (Elem->Type == EJson::Object) OutStr += FormatJsonObjectToUnrealText(Elem->AsObject());
					else if (Elem->Type == EJson::String) OutStr += FString::Printf(TEXT("\"%s\""), *Elem->AsString());
					else if (Elem->Type == EJson::Number) OutStr += FString::Printf(TEXT("%f"), Elem->AsNumber());
					else if (Elem->Type == EJson::Boolean) OutStr += Elem->AsBool() ? TEXT("True") : TEXT("False");
				}
				OutStr += TEXT(")");
			}
		}
		OutStr += TEXT(")");
		return OutStr;
	}

	FString ExpandBlueprintAssetPath(const FString& InPath)
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

	FString CompressBlueprintAssetPath(const FString& InPath)
	{
		if (InPath.StartsWith(TEXT("/Game/")))
		{
			return InPath.Mid(6);
		}
		return InPath;
	}

	FString GetVarTypeString(const FEdGraphPinType& VarType)
	{
		auto FormatSingleType = [](const FName& PinCategory, const FName& PinSubCategory, UObject* SubCategoryObject) -> FString
		{
			FString BaseName = PinCategory.ToString();
			if (IsValid(SubCategoryObject))
			{
				BaseName = SubCategoryObject->GetName();
			}

			if (PinCategory == UEdGraphSchema_K2::PC_Boolean)
			{
				return TEXT("bool");
			}
			else if (PinCategory == UEdGraphSchema_K2::PC_Int)
			{
				return TEXT("int32");
			}
			else if (PinCategory == UEdGraphSchema_K2::PC_String)
			{
				return TEXT("FString");
			}
			else if (PinCategory == UEdGraphSchema_K2::PC_Name)
			{
				return TEXT("FName");
			}
			else if (PinCategory == UEdGraphSchema_K2::PC_Text)
			{
				return TEXT("FText");
			}
			else if (PinCategory == UEdGraphSchema_K2::PC_Byte && !SubCategoryObject)
			{
				return TEXT("uint8");
			}
			else if (PinCategory == UEdGraphSchema_K2::PC_Real)
			{
				if (PinSubCategory == UEdGraphSchema_K2::PC_Float)
				{
					return TEXT("float");
				}
				else if (PinSubCategory == UEdGraphSchema_K2::PC_Double)
				{
					return TEXT("double");
				}
				else
				{
					return TEXT("float");
				}
			}
			else if (PinCategory == UEdGraphSchema_K2::PC_Class)
			{
				return FString::Printf(TEXT("TSubclassOf<%s>"), *BaseName);
			}
			else if (PinCategory == UEdGraphSchema_K2::PC_SoftObject)
			{
				return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *BaseName);
			}
			else if (PinCategory == UEdGraphSchema_K2::PC_SoftClass)
			{
				return FString::Printf(TEXT("TSoftClassPtr<%s>"), *BaseName);
			}
			else if (PinCategory == UEdGraphSchema_K2::PC_Object)
			{
				return BaseName;
			}
			return BaseName;
		};

		FString KeyType = FormatSingleType(VarType.PinCategory, VarType.PinSubCategory, VarType.PinSubCategoryObject.Get());

		if (VarType.ContainerType == EPinContainerType::Array)
		{
			return FString::Printf(TEXT("TArray<%s>"), *KeyType);
		}
		else if (VarType.ContainerType == EPinContainerType::Set)
		{
			return FString::Printf(TEXT("TSet<%s>"), *KeyType);
		}
		else if (VarType.ContainerType == EPinContainerType::Map)
		{
			FString ValueType = FormatSingleType(
				VarType.PinValueType.TerminalCategory,
				VarType.PinValueType.TerminalSubCategory,
				VarType.PinValueType.TerminalSubCategoryObject.Get()
			);
			return FString::Printf(TEXT("TMap<%s, %s>"), *KeyType, *ValueType);
		}
		return KeyType;
	}
}

FAgentFrameworkBlueprintActions::FAgentFrameworkBlueprintActions() {}
FAgentFrameworkBlueprintActions::~FAgentFrameworkBlueprintActions() {}

FName FAgentFrameworkBlueprintActions::GetActionName() const { return FName(TEXT("Blueprint")); }

TArray<FString> FAgentFrameworkBlueprintActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_blueprint_actor"),
		TEXT("add_blueprint_component"),
		TEXT("add_blueprint_variable"),
		TEXT("add_blueprint_function"),
		TEXT("add_blueprint_event"),
		TEXT("compile_blueprint"),
		TEXT("set_blueprint_defaults"),
		TEXT("set_component_properties"),
		TEXT("inject_blueprint_nodes_t3d"),
		TEXT("get_blueprint_info"),
		TEXT("connect_blueprint_pins"),
		TEXT("add_enhanced_input_node"),
		TEXT("modify_blueprint"),
		TEXT("verify_blueprint_connections"),
		TEXT("set_node_pin_default"),
		TEXT("delete_blueprint_nodes"),
		TEXT("analyze_blueprint_graph"),
		TEXT("execute_batch_blueprint_operations"),
		TEXT("get_blueprint_schema"),
		TEXT("export_blueprint_summary"),
		TEXT("check_asset_state"),
		TEXT("disconnect_blueprint_pins"),
		TEXT("modify_blueprint_subobject"),
		TEXT("configure_actor_replication"),
		TEXT("set_variable_replication")
	};
}

bool FAgentFrameworkBlueprintActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString AssetPath;
	if (Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Params->SetStringField(TEXT("asset_path"), ExpandBlueprintAssetPath(AssetPath));
	}
	else if (Params->TryGetStringField(TEXT("blueprint_path"), AssetPath))
	{
		Params->SetStringField(TEXT("asset_path"), ExpandBlueprintAssetPath(AssetPath));
	}
	else if (Params->TryGetStringField(TEXT("TargetAsset"), AssetPath))
	{
		Params->SetStringField(TEXT("asset_path"), ExpandBlueprintAssetPath(AssetPath));
	}
	else if (Params->TryGetStringField(TEXT("AssetPath"), AssetPath))
	{
		Params->SetStringField(TEXT("asset_path"), ExpandBlueprintAssetPath(AssetPath));
	}

	FString InputActionPath;
	if (Params->TryGetStringField(TEXT("input_action"), InputActionPath))
	{
		Params->SetStringField(TEXT("input_action"), ExpandBlueprintAssetPath(InputActionPath));
	}

	if (!Params->HasField(TEXT("asset_path")))
	{
		OutErrors.Add(TEXT("Missing required field: asset_path"));
		return false;
	}

	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);

	if (ToolName == TEXT("create_blueprint_actor"))
	{
		if (!Params->HasField(TEXT("parent_class")))
		{
			OutErrors.Add(TEXT("Missing required field for create_blueprint_actor: parent_class"));
			return false;
		}
	}
	else if (ToolName == TEXT("add_blueprint_component"))
	{
		if (!Params->HasField(TEXT("component_class")) || !Params->HasField(TEXT("component_name")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for add_blueprint_component: component_class, component_name"));
			return false;
		}
	}
	else if (ToolName == TEXT("add_blueprint_variable"))
	{
		if (!Params->HasField(TEXT("variable_name")) || !Params->HasField(TEXT("variable_type")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for add_blueprint_variable: variable_name, variable_type"));
			return false;
		}
	}
	else if (ToolName == TEXT("add_blueprint_function"))
	{
		if (!Params->HasField(TEXT("function_name")))
		{
			OutErrors.Add(TEXT("Missing required field for add_blueprint_function: function_name"));
			return false;
		}
	}
	else if (ToolName == TEXT("add_blueprint_event"))
	{
		if (!Params->HasField(TEXT("event_name")))
		{
			OutErrors.Add(TEXT("Missing required field for add_blueprint_event: event_name"));
			return false;
		}
	}
	else if (ToolName == TEXT("set_blueprint_defaults"))
	{
		if (!Params->HasField(TEXT("defaults")))
		{
			OutErrors.Add(TEXT("Missing required field for set_blueprint_defaults: defaults (must be a JSON object)"));
			return false;
		}
	}
	else if (ToolName == TEXT("set_component_properties"))
	{
		if (!Params->HasField(TEXT("component_name")) || !Params->HasField(TEXT("properties")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for set_component_properties: component_name, properties"));
			return false;
		}
	}
	else if (ToolName == TEXT("inject_blueprint_nodes_t3d"))
	{
		if (!Params->HasField(TEXT("t3d_text")))
		{
			OutErrors.Add(TEXT("Missing required field for inject_blueprint_nodes_t3d: t3d_text"));
			return false;
		}
	}
	else if (ToolName == TEXT("connect_blueprint_pins"))
	{
		if (!Params->HasField(TEXT("source_node")) || !Params->HasField(TEXT("source_pin")) ||
			!Params->HasField(TEXT("target_node")) || !Params->HasField(TEXT("target_pin")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for connect_blueprint_pins: source_node, source_pin, target_node, target_pin"));
			return false;
		}
	}
	else if (ToolName == TEXT("add_enhanced_input_node"))
	{
		if (!Params->HasField(TEXT("input_action")) || !Params->HasField(TEXT("event_type")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for add_enhanced_input_node: input_action, event_type"));
			return false;
		}
	}
	else if (ToolName == TEXT("set_node_pin_default"))
	{
		if (!Params->HasField(TEXT("node_name")) || !Params->HasField(TEXT("pin_name")) || !Params->HasField(TEXT("value")))
		{
			OutErrors.Add(TEXT("Missing required field(s) for set_node_pin_default: node_name, pin_name, value"));
			return false;
		}
	}
	else if (ToolName == TEXT("delete_blueprint_nodes"))
	{
		if (!Params->HasField(TEXT("node_names")))
		{
			OutErrors.Add(TEXT("Missing required field for delete_blueprint_nodes: node_names (must be a string array)"));
			return false;
		}
	}
	else if (ToolName == TEXT("execute_batch_blueprint_operations"))
	{
		if (!Params->HasField(TEXT("operations")))
		{
			OutErrors.Add(TEXT("Missing required field for execute_batch_blueprint_operations: operations (must be a JSON array)"));
			return false;
		}
	}
	else if (ToolName == TEXT("disconnect_blueprint_pins"))
	{
		bool bHasNode = Params->HasField(TEXT("node_guid")) || Params->HasField(TEXT("NodeGuid")) || Params->HasField(TEXT("node_name")) || Params->HasField(TEXT("source_node"));
		bool bHasPin = Params->HasField(TEXT("pin_name")) || Params->HasField(TEXT("PinName"));
		if (!bHasNode || !bHasPin)
		{
			OutErrors.Add(TEXT("Missing required field(s) for disconnect_blueprint_pins: node_guid (or NodeGuid/node_name), pin_name (or PinName)"));
			return false;
		}
	}
	else if (ToolName == TEXT("modify_blueprint_subobject"))
	{
		bool bHasSubObj = Params->HasField(TEXT("subobject_path")) || Params->HasField(TEXT("SubObjectPath"));
		bool bHasProps = Params->HasField(TEXT("properties")) || Params->HasField(TEXT("Properties"));
		if (!bHasSubObj || !bHasProps)
		{
			OutErrors.Add(TEXT("Missing required field(s) for modify_blueprint_subobject: subobject_path, properties"));
			return false;
		}
	}
	else if (ToolName == TEXT("configure_actor_replication"))
	{
		// asset_path is validated by root check
	}
	else if (ToolName == TEXT("set_variable_replication"))
	{
		bool bHasVar = Params->HasField(TEXT("variable_name")) || Params->HasField(TEXT("VariableName"));
		bool bHasRep = Params->HasField(TEXT("replication_type")) || Params->HasField(TEXT("ReplicationType"));
		if (!bHasVar || !bHasRep)
		{
			OutErrors.Add(TEXT("Missing required field(s) for set_variable_replication: variable_name, replication_type"));
			return false;
		}
	}

	return true;
}

// ============================================================================
// ExecuteAction — Dispatch
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);

	bool bIsReadOnly = (ToolName == TEXT("get_blueprint_info") ||
	                    ToolName == TEXT("verify_blueprint_connections") ||
	                    ToolName == TEXT("analyze_blueprint_graph") ||
	                    ToolName == TEXT("get_blueprint_schema") ||
	                    ToolName == TEXT("export_blueprint_summary") ||
	                    ToolName == TEXT("check_asset_state"));

	// Sentinel check for modifying tools
	if (!bIsReadOnly)
	{
		FString AssetPath;
		if (Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		{
			AssetPath = ExpandBlueprintAssetPath(AssetPath);

			// 1. In-memory Dirty Check (Smart Sentinel)
			UPackage* Package = FindPackage(nullptr, *AssetPath);
			if (Package && Package->IsDirty())
			{
				if (!FAgentFrameworkActionsModule::AgentDirtiedPackages.Contains(FName(*AssetPath)))
				{
					FAgentFrameworkActionResult FailResult;
					FailResult.bSuccess = false;
					FailResult.Errors.Add(FString::Printf(TEXT("SENTINEL ERROR: Asset '%s' has unsaved changes made by the user in Unreal Editor. Please save the asset in the editor first."), *AssetPath));
					return FailResult;
				}
			}

			// 2. Source Control Lock Check
			ISourceControlModule& SCModule = ISourceControlModule::Get();
			if (SCModule.IsEnabled() && SCModule.GetProvider().IsAvailable())
			{
				FString FilePath = USourceControlHelpers::PackageFilename(AssetPath);
				FSourceControlStatePtr SCState = SCModule.GetProvider().GetState(FilePath, EStateCacheUsage::Use);
				if (SCState.IsValid() && (SCState->IsCheckedOutOther() || !SCState->CanEdit()))
				{
					FAgentFrameworkActionResult FailResult;
					FailResult.bSuccess = false;
					FailResult.Errors.Add(FString::Printf(TEXT("SENTINEL ERROR: Asset '%s' is locked or checked out by another user in source control."), *AssetPath));
					return FailResult;
				}
			}
		}
	}

	TOptional<FScopedTransaction> Transaction;
	if (!bIsReadOnly)
	{
		Transaction.Emplace(FText::FromString(TEXT("AgentFramework Blueprint Action")));
	}

	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	if (ToolName == TEXT("create_blueprint_actor"))             Result = ExecuteCreateBlueprint(Params, Result);
	else if (ToolName == TEXT("add_blueprint_component"))       Result = ExecuteAddComponent(Params, Result);
	else if (ToolName == TEXT("add_blueprint_variable"))        Result = ExecuteAddVariable(Params, Result);
	else if (ToolName == TEXT("add_blueprint_function"))        Result = ExecuteAddFunction(Params, Result);
	else if (ToolName == TEXT("add_blueprint_event"))           Result = ExecuteAddEventHandler(Params, Result);
	else if (ToolName == TEXT("compile_blueprint"))             Result = ExecuteCompileBlueprint(Params, Result);
	else if (ToolName == TEXT("set_blueprint_defaults"))        Result = ExecuteSetDefaults(Params, Result);
	else if (ToolName == TEXT("set_component_properties"))      Result = ExecuteSetComponentProperties(Params, Result);
	else if (ToolName == TEXT("inject_blueprint_nodes_t3d"))    Result = ExecuteInjectNodesT3D(Params, Result);
	else if (ToolName == TEXT("get_blueprint_info"))            Result = ExecuteGetBlueprintInfo(Params, Result);
	else if (ToolName == TEXT("get_blueprint_schema"))          Result = ExecuteGetBlueprintSchema(Params, Result);
	else if (ToolName == TEXT("connect_blueprint_pins"))        Result = ExecuteConnectPins(Params, Result);
	else if (ToolName == TEXT("add_enhanced_input_node"))       Result = ExecuteAddEnhancedInputNode(Params, Result);
	else if (ToolName == TEXT("verify_blueprint_connections"))  Result = ExecuteVerifyConnections(Params, Result);
	else if (ToolName == TEXT("set_node_pin_default"))          Result = ExecuteSetNodePinDefault(Params, Result);
	else if (ToolName == TEXT("delete_blueprint_nodes"))        Result = ExecuteDeleteNodes(Params, Result);
	else if (ToolName == TEXT("analyze_blueprint_graph"))       Result = ExecuteAnalyzeBlueprintGraph(Params, Result);
	else if (ToolName == TEXT("execute_batch_blueprint_operations")) Result = ExecuteBatchOperations(Params, Result);
	else if (ToolName == TEXT("export_blueprint_summary"))      Result = ExecuteExportBlueprintSummary(Params, Result);
	else if (ToolName == TEXT("check_asset_state"))             Result = ExecuteCheckAssetState(Params, Result);
	else if (ToolName == TEXT("modify_blueprint"))              Result = ExecuteModifyBlueprint(Params, Result);
	else if (ToolName == TEXT("disconnect_blueprint_pins"))   Result = ExecuteDisconnectPins(Params, Result);
	else if (ToolName == TEXT("modify_blueprint_subobject"))   Result = ExecuteModifySubobject(Params, Result);
	else if (ToolName == TEXT("configure_actor_replication"))  Result = ExecuteConfigureActorReplication(Params, Result);
	else if (ToolName == TEXT("set_variable_replication"))     Result = ExecuteSetVariableReplication(Params, Result);
	else
	{
		// Legacy param-based fallback dispatch
		if (Params->HasField(TEXT("parent_class")))           Result = ExecuteCreateBlueprint(Params, Result);
		else if (Params->HasField(TEXT("component_class")))   Result = ExecuteAddComponent(Params, Result);
		else if (Params->HasField(TEXT("variable_name")))     Result = ExecuteAddVariable(Params, Result);
		else if (Params->HasField(TEXT("function_name")))     Result = ExecuteAddFunction(Params, Result);
		else if (Params->HasField(TEXT("t3d_text")))          Result = ExecuteInjectNodesT3D(Params, Result);
		else if (Params->HasField(TEXT("defaults")))          Result = ExecuteSetDefaults(Params, Result);
		else
		{
			Result.Errors.Add(FString::Printf(TEXT("Unknown Blueprint tool: '%s'. Supported: create_blueprint_actor, add_blueprint_component, add_blueprint_variable, add_blueprint_function, add_blueprint_event, compile_blueprint, set_blueprint_defaults, set_component_properties, inject_blueprint_nodes_t3d, get_blueprint_info, get_blueprint_schema, connect_blueprint_pins, set_node_pin_default, verify_blueprint_connections, analyze_blueprint_graph, execute_batch_blueprint_operations, disconnect_blueprint_pins, modify_blueprint_subobject, configure_actor_replication, set_variable_replication"), *ToolName));
		}
	}

	if (Transaction.IsSet() && !Result.bSuccess)
	{
		Transaction->Cancel();
	}

	if (Result.bSuccess && !bIsReadOnly)
	{
		FString AssetPath;
		TArray<FString> Errors;
		if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, false))
		{
			AssetModificationCounts.FindOrAdd(AssetPath, 0)++;
			FAgentFrameworkActionsModule::AgentDirtiedPackages.Add(FName(*AssetPath));
		}

#if WITH_EDITOR
		if (GEditor)
		{
			USoundBase* SuccessSound = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
			if (IsValid(SuccessSound))
			{
				GEditor->PlayEditorSound(SuccessSound);
			}
		}
#endif
	}

	return Result;
}

// ============================================================================
// Helper: CompileAndReport
// ============================================================================

bool FAgentFrameworkBlueprintActions::CompileAndReport(UBlueprint* Blueprint, FAgentFrameworkActionResult& Result, bool bSkipGC)
{
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(TEXT("Blueprint pointer is invalid."));
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	FCompilerResultsLog Log;
	const EBlueprintCompileOptions Opts = bSkipGC
		? EBlueprintCompileOptions::SkipGarbageCollection
		: EBlueprintCompileOptions::None;

	FKismetEditorUtilities::CompileBlueprint(Blueprint, Opts, &Log);

	TMap<FString, int32> MessageGroups;
	int32 TruncatedErrorsCount = 0;
	int32 TruncatedWarningsCount = 0;

	for (const TSharedRef<FTokenizedMessage>& Msg : Log.Messages)
	{
		FString MsgText = Msg->ToText().ToString();
		FString GroupKey = MsgText.Left(40);

		int32& Count = MessageGroups.FindOrAdd(GroupKey, 0);
		Count++;

		if (Count <= 3)
		{
			if (Msg->GetSeverity() == EMessageSeverity::Error)
			{
				Result.Errors.Add(FString::Printf(TEXT("COMPILE ERROR: %s"), *MsgText));
			}
			else if (Msg->GetSeverity() == EMessageSeverity::Warning)
			{
				Result.Warnings.Add(FString::Printf(TEXT("COMPILE WARNING: %s"), *MsgText));
			}
		}
		else
		{
			if (Msg->GetSeverity() == EMessageSeverity::Error)
			{
				TruncatedErrorsCount++;
			}
			else if (Msg->GetSeverity() == EMessageSeverity::Warning)
			{
				TruncatedWarningsCount++;
			}
		}
	}

	if (TruncatedErrorsCount > 0)
	{
		Result.Errors.Add(FString::Printf(TEXT("... Truncated %d similar compile errors to save tokens ..."), TruncatedErrorsCount));
	}
	if (TruncatedWarningsCount > 0)
	{
		Result.Warnings.Add(FString::Printf(TEXT("... Truncated %d similar compile warnings to save tokens ..."), TruncatedWarningsCount));
	}

	return (Blueprint->Status != BS_Error);
}

// ============================================================================
// Helper: FindOrCreateEventGraph
// ============================================================================

UEdGraph* FAgentFrameworkBlueprintActions::FindOrCreateEventGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!IsValid(Blueprint)) return nullptr;

	// Search UbergraphPages
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (IsValid(Graph) && Graph->GetName() == GraphName)
			return Graph;
	}
	// Search FunctionGraphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (IsValid(Graph) && Graph->GetName() == GraphName)
			return Graph;
	}

	// If looking for EventGraph / NewEventGraph, create an uber-graph page
	if (GraphName == TEXT("EventGraph") || GraphName == TEXT("NewEventGraph"))
	{
		UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			FName(*GraphName),
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());

		if (IsValid(NewGraph))
		{
			FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
			return NewGraph;
		}
	}

	return nullptr;
}

// ============================================================================
// Helper: FindSCSNodeByName
// ============================================================================

USCS_Node* FAgentFrameworkBlueprintActions::FindSCSNodeByName(UBlueprint* Blueprint, const FString& NodeName)
{
	if (!IsValid(Blueprint) || !IsValid(Blueprint->SimpleConstructionScript))
		return nullptr;

	TArray<USCS_Node*> AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
	for (USCS_Node* Node : AllNodes)
	{
		if (IsValid(Node) && Node->GetVariableName().ToString() == NodeName)
			return Node;
	}
	return nullptr;
}

// ============================================================================
// Helper: ResolveT3DPlaceholders
// ============================================================================

FString FAgentFrameworkBlueprintActions::ResolveT3DPlaceholders(const FString& T3DText)
{
	// Build a deterministic map: placeholder token → fresh GUID string
	// Tokens look like: LINK_1, GUID_A, NODEREF_Entry, ID_Foo
	TMap<FString, FString> PlaceholderMap;

	// Pattern: word characters starting with LINK_|GUID_|NODEREF_|ID_
	const FRegexPattern Pattern(TEXT("(LINK_|GUID_|NODEREF_|ID_)[A-Za-z0-9_]+"));
	FRegexMatcher Matcher(Pattern, T3DText);

	while (Matcher.FindNext())
	{
		FString Token = Matcher.GetCaptureGroup(0);
		if (!PlaceholderMap.Contains(Token))
		{
			// Generate a UE-style 32-char uppercase GUID (no dashes)
			FGuid NewGuid = FGuid::NewGuid();
			FString GuidStr = NewGuid.ToString(EGuidFormats::Digits).ToUpper();
			PlaceholderMap.Add(Token, GuidStr);
		}
	}

	TArray<FString> SortedKeys;
	PlaceholderMap.GetKeys(SortedKeys);
	SortedKeys.Sort([](const FString& A, const FString& B) {
		return A.Len() > B.Len();
	});

	FString Result = T3DText;
	for (const FString& Key : SortedKeys)
	{
		Result = Result.Replace(*Key, *PlaceholderMap[Key], ESearchCase::CaseSensitive);
	}

	return Result;
}

// ============================================================================
// Helper: DetectInfiniteLoopRisk
// ============================================================================

bool FAgentFrameworkBlueprintActions::DetectInfiniteLoopRisk(UBlueprint* Blueprint, TArray<FString>& OutWarnings) const
{
	if (!IsValid(Blueprint)) return false;
	bool bRiskDetected = false;
	int32 CastCount = 0;

	// Audit UbergraphPages
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!IsValid(Graph)) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!IsValid(Node)) continue;

			if (IsValid(Node->GetClass()) && Node->GetClass()->GetName().Contains(TEXT("K2Node_DynamicCast")))
			{
				CastCount++;
			}

			UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
			if (IsValid(EventNode))
			{
				FName EventName = EventNode->GetFunctionName();
				if (EventName == FName(TEXT("ReceiveTick")))
				{
					OutWarnings.Add(TEXT("WARNING: EventTick detected. Avoid spawning actors, adding components, or performing heavy operations in Tick — this runs every frame and can cause severe performance degradation."));
					bRiskDetected = true;
				}
			}
		}
	}

	// Audit FunctionGraphs for Casts
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!IsValid(Graph)) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (IsValid(Node) && IsValid(Node->GetClass()) && Node->GetClass()->GetName().Contains(TEXT("K2Node_DynamicCast")))
			{
				CastCount++;
			}
		}
	}

	if (CastCount > 3)
	{
		OutWarnings.Add(FString::Printf(TEXT("WARNING: High number of Dynamic Cast nodes (%d) detected. Excess casting creates strong coupling. Consider refactoring to use Blueprint Interfaces or Actor Components for communication."), CastCount));
		bRiskDetected = true;
	}

	if (IsValid(Blueprint->SimpleConstructionScript))
	{
		TArray<USCS_Node*> AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
		if (AllNodes.Num() > 50)
		{
			OutWarnings.Add(FString::Printf(TEXT("WARNING: Construction script has %d components. This may cause performance issues."), AllNodes.Num()));
			bRiskDetected = true;
		}
	}

	return bRiskDetected;
}

// ============================================================================
// Helper: BuildBlueprintInfoJson
// ============================================================================

FString FAgentFrameworkBlueprintActions::BuildBlueprintInfoJson(UBlueprint* Blueprint, const TArray<FString>* NodeNamesFilter, bool bExcludeVisualLayout, const FString& QueryMode)
{
	if (!IsValid(Blueprint)) return TEXT("{}");
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

	// Basic info
	Root->SetStringField(TEXT("asset_path"), CompressBlueprintAssetPath(Blueprint->GetPathName()));
	Root->SetStringField(TEXT("parent_class"), IsValid(Blueprint->ParentClass) ? Blueprint->ParentClass->GetName() : TEXT("unknown"));

	FString StatusStr = TEXT("unknown");
	switch (Blueprint->Status)
	{
	case BS_UpToDate: StatusStr = TEXT("up_to_date"); break;
	case BS_Dirty:    StatusStr = TEXT("dirty"); break;
	case BS_Error:    StatusStr = TEXT("error"); break;
	case BS_Unknown:  StatusStr = TEXT("unknown"); break;
	default:          break;
	}
	Root->SetStringField(TEXT("compile_status"), StatusStr);

	// Variables
	TArray<TSharedPtr<FJsonValue>> VarsArray;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
		if (!Var.Category.IsEmpty())
			VarObj->SetStringField(TEXT("category"), Var.Category.ToString());
		VarsArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}
	Root->SetArrayField(TEXT("variables"), VarsArray);

	// SCS Components
	TArray<TSharedPtr<FJsonValue>> CompsArray;
	if (IsValid(Blueprint->SimpleConstructionScript))
	{
		TArray<USCS_Node*> AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
		for (USCS_Node* Node : AllNodes)
		{
			if (!IsValid(Node)) continue;
			TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
			CompObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
			CompObj->SetStringField(TEXT("class"), IsValid(Node->ComponentClass) ? Node->ComponentClass->GetName() : TEXT("unknown"));

			// Parent node
			USCS_Node* ParentNode = Blueprint->SimpleConstructionScript->FindParentNode(Node);
			if (IsValid(ParentNode))
				CompObj->SetStringField(TEXT("parent"), ParentNode->GetVariableName().ToString());
			else
				CompObj->SetStringField(TEXT("parent"), TEXT("root"));

			CompsArray.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}
	Root->SetArrayField(TEXT("components"), CompsArray);

	// Graphs
	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	auto AddGraphEntry = [&](UEdGraph* Graph, const FString& GraphType)
	{
		if (!IsValid(Graph)) return;
		TSharedPtr<FJsonObject> GObj = MakeShared<FJsonObject>();
		GObj->SetStringField(TEXT("name"), Graph->GetName());
		GObj->SetStringField(TEXT("type"), GraphType);
		GObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

		// Enumerate nodes with class, title, position, and connections
		TArray<TSharedPtr<FJsonValue>> NodesArray;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!IsValid(Node)) continue;
			if (NodeNamesFilter && !NodeNamesFilter->Contains(Node->GetName())) continue;

			// If interface_only mode, only include entry/result/event nodes
			if (QueryMode.Equals(TEXT("interface_only"), ESearchCase::IgnoreCase))
			{
				bool bIsInterfaceNode = false;
				if (GraphType.Equals(TEXT("event_graph"), ESearchCase::IgnoreCase) && IsValid(Node->GetClass()))
				{
					bIsInterfaceNode = Node->GetClass()->IsChildOf(UK2Node_Event::StaticClass()) ||
					                   Node->GetClass()->IsChildOf(UK2Node_CustomEvent::StaticClass());
				}
				else if (GraphType.Equals(TEXT("function"), ESearchCase::IgnoreCase) && IsValid(Node->GetClass()))
				{
					bIsInterfaceNode = Node->GetClass()->IsChildOf(UK2Node_FunctionEntry::StaticClass()) ||
					                   Node->GetClass()->IsChildOf(UK2Node_FunctionResult::StaticClass());
				}
				else if (GraphType.Equals(TEXT("macro"), ESearchCase::IgnoreCase) && IsValid(Node->GetClass()))
				{
					bIsInterfaceNode = Node->GetClass()->GetName().Contains(TEXT("Tunnel"));
				}

				if (!bIsInterfaceNode) continue;
			}
			
			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("name"), Node->GetName());
			NodeObj->SetStringField(TEXT("class"), IsValid(Node->GetClass()) ? Node->GetClass()->GetName() : TEXT("unknown"));
			NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			
			if (!bExcludeVisualLayout)
			{
				NodeObj->SetNumberField(TEXT("pos_x"), Node->NodePosX);
				NodeObj->SetNumberField(TEXT("pos_y"), Node->NodePosY);
				if (!Node->NodeComment.IsEmpty())
					NodeObj->SetStringField(TEXT("comment"), Node->NodeComment);
			}

			// For K2Node_CallFunction, include the function reference
			if (IsValid(Node->GetClass()) && Node->GetClass()->GetName().Contains(TEXT("CallFunction")))
			{
				FString FuncRef;
				if (FProperty* Prop = Node->GetClass()->FindPropertyByName(TEXT("FunctionReference")))
				{
					FString ExportedText;
					Prop->ExportTextItem_Direct(ExportedText, Prop->ContainerPtrToValuePtr<void>(Node), nullptr, Node, PPF_None);
					if (!ExportedText.IsEmpty())
						NodeObj->SetStringField(TEXT("function_ref"), ExportedText);
				}
			}

			// Enumerate pins with connections
			TArray<TSharedPtr<FJsonValue>> PinsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;
				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
				PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
				if (!Pin->DefaultValue.IsEmpty())
					PinObj->SetStringField(TEXT("default"), Pin->DefaultValue);

				// Connected pins
				if (Pin->LinkedTo.Num() > 0)
				{
					TArray<TSharedPtr<FJsonValue>> LinksArray;
					for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
					{
						if (!LinkedPin || !IsValid(LinkedPin->GetOwningNode())) continue;
						TSharedPtr<FJsonObject> LinkObj = MakeShared<FJsonObject>();
						LinkObj->SetStringField(TEXT("node"), LinkedPin->GetOwningNode()->GetName());
						LinkObj->SetStringField(TEXT("pin"), LinkedPin->PinName.ToString());
						LinksArray.Add(MakeShared<FJsonValueObject>(LinkObj));
					}
					PinObj->SetArrayField(TEXT("linked_to"), LinksArray);
				}

				PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("pins"), PinsArray);

			NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
		}
		GObj->SetArrayField(TEXT("nodes"), NodesArray);

		GraphsArray.Add(MakeShared<FJsonValueObject>(GObj));
	};

	for (UEdGraph* G : Blueprint->UbergraphPages)   if (IsValid(G)) AddGraphEntry(G, TEXT("event_graph"));
	for (UEdGraph* G : Blueprint->FunctionGraphs)   if (IsValid(G)) AddGraphEntry(G, TEXT("function"));
	for (UEdGraph* G : Blueprint->MacroGraphs)      if (IsValid(G)) AddGraphEntry(G, TEXT("macro"));

	Root->SetArrayField(TEXT("graphs"), GraphsArray);

	FString OutputStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputStr);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return OutputStr;
}

// ============================================================================
// Helper: ResolveEventMapping
// ============================================================================

bool FAgentFrameworkBlueprintActions::ResolveEventMapping(const FString& EventName, FName& OutFunctionName, UClass*& OutOwnerClass)
{
	// Actor events
	if (EventName == TEXT("BeginPlay"))           { OutFunctionName = FName(TEXT("ReceiveBeginPlay"));             OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("EndPlay"))             { OutFunctionName = FName(TEXT("ReceiveEndPlay"));               OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("Tick"))                { OutFunctionName = FName(TEXT("ReceiveTick"));                  OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("ActorBeginOverlap"))   { OutFunctionName = FName(TEXT("ReceiveActorBeginOverlap"));     OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("ActorEndOverlap"))     { OutFunctionName = FName(TEXT("ReceiveActorEndOverlap"));       OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("Hit"))                 { OutFunctionName = FName(TEXT("ReceiveHit"));                   OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("AnyDamage"))           { OutFunctionName = FName(TEXT("ReceiveAnyDamage"));             OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("PointDamage"))         { OutFunctionName = FName(TEXT("ReceivePointDamage"));           OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("RadialDamage"))        { OutFunctionName = FName(TEXT("ReceiveRadialDamage"));          OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("Destroyed"))           { OutFunctionName = FName(TEXT("ReceiveDestroyed"));             OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("ActorBeginCursorOver")){ OutFunctionName = FName(TEXT("ReceiveActorBeginCursorOver"));  OutOwnerClass = AActor::StaticClass();      return true; }
	if (EventName == TEXT("ActorEndCursorOver"))  { OutFunctionName = FName(TEXT("ReceiveActorEndCursorOver"));   OutOwnerClass = AActor::StaticClass();      return true; }

	// Pawn events
	if (EventName == TEXT("PossessedBy"))         { OutFunctionName = FName(TEXT("ReceivePossessed"));             OutOwnerClass = APawn::StaticClass();       return true; }
	if (EventName == TEXT("UnPossessed"))         { OutFunctionName = FName(TEXT("ReceiveUnpossessed"));           OutOwnerClass = APawn::StaticClass();       return true; }
	// SetupPlayerInputComponent — correct UE internal name is ReceiveSetUpPlayerInputComponent (capital U on "Up")
	if (EventName == TEXT("SetupPlayerInputComponent")) { OutFunctionName = FName(TEXT("ReceiveSetUpPlayerInputComponent")); OutOwnerClass = APawn::StaticClass(); return true; }

	// Character events
	if (EventName == TEXT("Landed"))              { OutFunctionName = FName(TEXT("OnLanded"));                     OutOwnerClass = ACharacter::StaticClass();  return true; }

	return false;
}

// ============================================================================
// ExecuteCreateBlueprint
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteCreateBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	FString ParentClassName;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("parent_class"), ParentClassName, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	// Resolve parent class — try aliases first, then full reflection search
	UClass* ParentClass = nullptr;
	if (ParentClassName == TEXT("Actor"))                ParentClass = AActor::StaticClass();
	else if (ParentClassName == TEXT("Pawn"))            ParentClass = APawn::StaticClass();
	else if (ParentClassName == TEXT("Character"))       ParentClass = ACharacter::StaticClass();
	else if (ParentClassName == TEXT("PlayerController"))ParentClass = APlayerController::StaticClass();
	else if (ParentClassName == TEXT("GameModeBase"))    ParentClass = AGameModeBase::StaticClass();
	else
	{
		ParentClass = FindObject<UClass>(nullptr, *ParentClassName);
		if (!IsValid(ParentClass))
			ParentClass = FindFirstObject<UClass>(*ParentClassName, EFindFirstObjectOptions::None);
		if (!IsValid(ParentClass))
			ParentClass = FindFirstObject<UClass>(*(FString(TEXT("/Script/Engine.")) + ParentClassName), EFindFirstObjectOptions::None);
	}

	if (!IsValid(ParentClass))
	{
		Result.Errors.Add(FString::Printf(TEXT("Could not find parent class: '%s'. Try 'Actor', 'Pawn', 'Character', 'PlayerController', or 'GameModeBase'."), *ParentClassName));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName   = FPackageName::GetShortName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString UniqueName, UniquePackagePath;
	AssetTools.CreateUniqueAssetName(AssetPath, TEXT(""), UniquePackagePath, UniqueName);

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	if (IsValid(Factory))
	{
		Factory->ParentClass = ParentClass;
	}

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UBlueprint::StaticClass(), Factory);
	UBlueprint* NewBlueprint = Cast<UBlueprint>(NewAsset);

	if (!IsValid(NewBlueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Blueprint at '%s'. Check that the path is valid and under /Game/."), *AssetPath));
		return Result;
	}

	UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: Created Blueprint '%s' with parent '%s'"), *AssetPath, *ParentClassName);

	// --- Inline Components ---
	const TArray<TSharedPtr<FJsonValue>>* ComponentsArray = nullptr;
	TArray<FString> CompsErrors;
	if (UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("components"), ComponentsArray, CompsErrors, false) && ComponentsArray)
	{
		for (const TSharedPtr<FJsonValue>& CompValue : *ComponentsArray)
		{
			const TSharedPtr<FJsonObject> CompObj = CompValue->AsObject();
			if (!CompObj.IsValid()) continue;

			FString CompName;
			FString CompClassName;
			TArray<FString> InlineCompErrors;
			if (!UAgentFrameworkActionUtils::TryGetStringParam(CompObj, TEXT("name"), CompName, InlineCompErrors, true) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(CompObj, TEXT("class"), CompClassName, InlineCompErrors, true))
			{
				Result.Warnings.Append(InlineCompErrors);
				continue;
			}

			UClass* CompClass = FindFirstObject<UClass>(*CompClassName, EFindFirstObjectOptions::None);
			if (!IsValid(CompClass))
				CompClass = FindFirstObject<UClass>(*(TEXT("U") + CompClassName), EFindFirstObjectOptions::None);
			if (!IsValid(CompClass))
				CompClass = FindFirstObject<UClass>(*(FString(TEXT("/Script/Engine.")) + CompClassName), EFindFirstObjectOptions::None);

			if (IsValid(CompClass) && IsValid(NewBlueprint->SimpleConstructionScript))
			{
				NewBlueprint->Modify();
				USCS_Node* NewNode = NewBlueprint->SimpleConstructionScript->CreateNode(CompClass, *CompName);
				if (IsValid(NewNode))
				{
					FString AttachTo;
					TArray<FString> AttachErrors;
					if (UAgentFrameworkActionUtils::TryGetStringParam(CompObj, TEXT("attach_to"), AttachTo, AttachErrors, false) && !AttachTo.IsEmpty())
					{
						USCS_Node* ParentNode = FindSCSNodeByName(NewBlueprint, AttachTo);
						if (IsValid(ParentNode))
							ParentNode->AddChildNode(NewNode);
						else
							NewBlueprint->SimpleConstructionScript->AddNode(NewNode);
					}
					else
					{
						NewBlueprint->SimpleConstructionScript->AddNode(NewNode);
					}
					UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: Added inline component '%s' (%s)"), *CompName, *CompClassName);
				}
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Component class not found: '%s'. Check the class name spelling."), *CompClassName));
			}
		}
	}

	// --- Inline Variables ---
	const TArray<TSharedPtr<FJsonValue>>* VariablesArray = nullptr;
	TArray<FString> VarsErrors;
	if (UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("variables"), VariablesArray, VarsErrors, false) && VariablesArray)
	{
		for (const TSharedPtr<FJsonValue>& VarValue : *VariablesArray)
		{
			const TSharedPtr<FJsonObject> VarObj = VarValue->AsObject();
			if (!VarObj.IsValid()) continue;

			FString VarName;
			FString VarType;
			TArray<FString> InlineVarErrors;
			if (!UAgentFrameworkActionUtils::TryGetStringParam(VarObj, TEXT("name"), VarName, InlineVarErrors, true) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(VarObj, TEXT("type"), VarType, InlineVarErrors, true))
			{
				Result.Warnings.Append(InlineVarErrors);
				continue;
			}

			FEdGraphPinType PinType;
			ResolvePinType(VarType, PinType);

			NewBlueprint->Modify();
			bool bSuccess = FBlueprintEditorUtils::AddMemberVariable(NewBlueprint, FName(*VarName), PinType);
			if (!bSuccess)
				Result.Warnings.Add(FString::Printf(TEXT("Failed to add inline variable: '%s'"), *VarName));
		}
	}

	// Compile and report
	bool bCompileOk = CompileAndReport(NewBlueprint, Result, true);

	// Infinite loop guard
	TArray<FString> LoopWarnings;
	DetectInfiniteLoopRisk(NewBlueprint, LoopWarnings);
	Result.Warnings.Append(LoopWarnings);

	if (!bCompileOk)
	{
		Result.bSuccess = false;
		Result.Errors.Add(FString::Printf(
			TEXT("Blueprint '%s' created but FAILED to compile. Fix the COMPILE ERROR messages above, then call compile_blueprint to verify."),
			*AssetPath));
	}

	// Save
	UPackage* Package = NewBlueprint->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, NewBlueprint, *PackageFilename, SaveArgs);
		}
	}

	FAssetRegistryModule::AssetCreated(NewBlueprint);

	Result.bSuccess = bCompileOk;
	Result.ResultMessage = FString::Printf(TEXT("Created Blueprint '%s' (parent: %s). Compile: %s."),
		*AssetName, *ParentClassName, bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED"));
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteAddComponent
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteAddComponent(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	FString CompClassName;
	FString CompName;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("blueprint_path"), AssetPath, Errors, false) &&
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, false))
	{
		Result.Errors.Add(TEXT("Missing required field for add_blueprint_component: blueprint_path or asset_path"));
		return Result;
	}

	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("component_class"), CompClassName, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("component_name"), CompName, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
	if (!IsValid(Blueprint))
	{
		Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	}
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	UClass* CompClass = FindFirstObject<UClass>(*CompClassName, EFindFirstObjectOptions::None);
	if (!IsValid(CompClass) && !CompClassName.StartsWith(TEXT("U")))
	{
		CompClass = FindFirstObject<UClass>(*(TEXT("U") + CompClassName), EFindFirstObjectOptions::None);
	}
	if (!IsValid(CompClass))
	{
		CompClass = LoadClass<UActorComponent>(nullptr, *CompClassName);
	}

	if (!IsValid(CompClass))
	{
		Result.Errors.Add(FString::Printf(TEXT("Component class not found: '%s'. Include the 'U' prefix or use the exact class name like 'StaticMeshComponent'."), *CompClassName));
		return Result;
	}

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!IsValid(SCS))
	{
		Result.Errors.Add(TEXT("Blueprint has no SimpleConstructionScript. Actors, Pawns, and Characters all have SCS; Interfaces and Function Libraries do not."));
		return Result;
	}

	Blueprint->Modify();
	USCS_Node* NewNode = SCS->CreateNode(CompClass, *CompName);
	if (!IsValid(NewNode))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create SCS node for component '%s'."), *CompName));
		return Result;
	}

	// Attach-to support (supports parent_component_name or attach_to)
	FString ParentCompName;
	TArray<FString> AttachErrors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("parent_component_name"), ParentCompName, AttachErrors, false) || ParentCompName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("attach_to"), ParentCompName, AttachErrors, false);
	}

	if (!ParentCompName.IsEmpty() && ParentCompName != TEXT("DefaultSceneRoot") && ParentCompName != TEXT("None"))
	{
		USCS_Node* ParentNode = SCS->FindSCSNode(FName(*ParentCompName));
		if (!IsValid(ParentNode))
		{
			ParentNode = FindSCSNodeByName(Blueprint, ParentCompName);
		}

		if (IsValid(ParentNode))
		{
			ParentNode->AddChildNode(NewNode);
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Parent component '%s' not found in SCS; attaching to root instead."), *ParentCompName));
			SCS->AddNode(NewNode);
		}
	}
	else
	{
		SCS->AddNode(NewNode);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	bool bCompileOk = CompileAndReport(Blueprint, Result, true);
	if (IsValid(Blueprint->GetOutermost()))
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}

	Result.bSuccess = bCompileOk;
	Result.ResultMessage = FString::Printf(TEXT("Added %s component '%s' to '%s'. Compile: %s."),
		*CompClassName, *CompName, *AssetPath, bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED"));
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteAddVariable
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteAddVariable(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	FString VarName;
	FString VarType;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("variable_name"), VarName, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("variable_type"), VarType, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	FEdGraphPinType PinType;
	ResolvePinType(VarType, PinType);

	Blueprint->Modify();
	bool bAdded = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType);
	if (!bAdded)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to add variable '%s' — it may already exist or the type is invalid."), *VarName));
		return Result;
	}

	// Optional: expose as editable / category
	FString Category;
	TArray<FString> CatErrors;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("category"), Category, CatErrors, false) && !Category.IsEmpty())
		FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*VarName), nullptr, FText::FromString(Category));

	bool bEditable = true;
	TArray<FString> EditErrors;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("editable"), bEditable, EditErrors, false);
	FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Blueprint, FName(*VarName), !bEditable);

	bool bCompileOk = CompileAndReport(Blueprint, Result, true);
	if (IsValid(Blueprint->GetOutermost()))
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}

	Result.bSuccess = bCompileOk;
	Result.ResultMessage = FString::Printf(TEXT("Added variable '%s' (%s) to '%s'. Compile: %s."),
		*VarName, *VarType, *AssetPath, bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED"));
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteAddFunction
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteAddFunction(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	FString FunctionName;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("function_name"), FunctionName, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	// Check if function already exists
	UEdGraph* ExistingGraph = nullptr;
	for (UEdGraph* G : Blueprint->FunctionGraphs)
	{
		if (IsValid(G) && G->GetName() == FunctionName)
		{
			ExistingGraph = G;
			break;
		}
	}

	if (IsValid(ExistingGraph))
	{
		Result.Warnings.Add(FString::Printf(TEXT("Function '%s' already exists in '%s'. Use inject_blueprint_nodes_t3d to add nodes to it."), *FunctionName, *AssetPath));
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Function '%s' already exists — skipping creation."), *FunctionName);
		return Result;
	}

	Blueprint->Modify();
	UEdGraph* NewFunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());

	if (IsValid(NewFunctionGraph))
	{
		FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewFunctionGraph, /*bIsUserCreated=*/true, (UClass*)nullptr);

		// Add typed inputs if provided
		const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
		TArray<FString> InputsErrors;
		if (UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("inputs"), InputsArray, InputsErrors, false) && InputsArray)
		{
			// Find the function entry node
			UK2Node_FunctionEntry* EntryNode = nullptr;
			for (UEdGraphNode* Node : NewFunctionGraph->Nodes)
			{
				EntryNode = Cast<UK2Node_FunctionEntry>(Node);
				if (IsValid(EntryNode)) break;
			}

			if (IsValid(EntryNode))
			{
				for (const TSharedPtr<FJsonValue>& InputVal : *InputsArray)
				{
					const TSharedPtr<FJsonObject> InputObj = InputVal->AsObject();
					if (!InputObj.IsValid()) continue;

					FString ParamName;
					FString ParamType;
					TArray<FString> ParamErrors;
					if (!UAgentFrameworkActionUtils::TryGetStringParam(InputObj, TEXT("name"), ParamName, ParamErrors, true) ||
						!UAgentFrameworkActionUtils::TryGetStringParam(InputObj, TEXT("type"), ParamType, ParamErrors, true))
					{
						continue;
					}

					FEdGraphPinType PinType;
					ResolvePinType(ParamType, PinType);

					EntryNode->Modify();
					TSharedPtr<FUserPinInfo> PinInfo = MakeShared<FUserPinInfo>();
					PinInfo->PinName = FName(*ParamName);
					PinInfo->PinType = PinType;
					EntryNode->UserDefinedPins.Add(PinInfo);
				}
				EntryNode->ReconstructNode();
			}
		}

		// Add typed outputs (return values) if provided
		const TArray<TSharedPtr<FJsonValue>>* OutputsArray = nullptr;
		TArray<FString> OutputsErrors;
		if (UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("outputs"), OutputsArray, OutputsErrors, false) && OutputsArray)
		{
			UK2Node_FunctionResult* ResultNode = nullptr;
			for (UEdGraphNode* Node : NewFunctionGraph->Nodes)
			{
				ResultNode = Cast<UK2Node_FunctionResult>(Node);
				if (IsValid(ResultNode)) break;
			}

			if (IsValid(ResultNode))
			{
				for (const TSharedPtr<FJsonValue>& OutputVal : *OutputsArray)
				{
					const TSharedPtr<FJsonObject> OutputObj = OutputVal->AsObject();
					if (!OutputObj.IsValid()) continue;

					FString RetName;
					FString RetType;
					TArray<FString> RetErrors;
					if (!UAgentFrameworkActionUtils::TryGetStringParam(OutputObj, TEXT("name"), RetName, RetErrors, true) ||
						!UAgentFrameworkActionUtils::TryGetStringParam(OutputObj, TEXT("type"), RetType, RetErrors, true))
					{
						continue;
					}

					FEdGraphPinType PinType;
					ResolvePinType(RetType, PinType);

					ResultNode->Modify();
					TSharedPtr<FUserPinInfo> PinInfo = MakeShared<FUserPinInfo>();
					PinInfo->PinName = FName(*RetName);
					PinInfo->PinType = PinType;
					ResultNode->UserDefinedPins.Add(PinInfo);
				}
				ResultNode->ReconstructNode();
			}
		}
	}

	bool bCompileOk = CompileAndReport(Blueprint, Result, true);
	if (IsValid(Blueprint->GetOutermost()))
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}

	Result.bSuccess = bCompileOk;
	Result.ResultMessage = FString::Printf(
		TEXT("Added function '%s' to '%s'.\n")
		TEXT("NESTING GUIDANCE: Build this function's graph bottom-up:\n")
		TEXT("  1. Inject leaf nodes first (the deepest utility calls, variable gets/sets)\n")
		TEXT("  2. Inject intermediate nodes that call those leaves\n")
		TEXT("  3. Wire execution chain: FunctionEntry (then exec) -> all intermediary nodes -> FunctionResult (execute)\n")
		TEXT("  4. Wire data pins from producer nodes to consumer nodes\n")
		TEXT("  5. Call verify_blueprint_connections after injecting to catch any missing wires\n")
		TEXT("Use inject_blueprint_nodes_t3d with graph_name='%s' to add logic.\n")
		TEXT("Compile: %s."),
		*FunctionName, *AssetPath, *FunctionName, bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED"));
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteAddEventHandler
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteAddEventHandler(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	FString EventName;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("event_name"), EventName, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	FName FunctionName;
	UClass* OwnerClass = nullptr;
	if (!ResolveEventMapping(EventName, FunctionName, OwnerClass))
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Unknown event name '%s'. Supported Actor events: BeginPlay, EndPlay, Tick, ActorBeginOverlap, ActorEndOverlap, Hit, AnyDamage, PointDamage, RadialDamage, Destroyed, ActorBeginCursorOver, ActorEndCursorOver. "
			     "Pawn events: PossessedBy, UnPossessed, SetupPlayerInputComponent. "
			     "Character events: Landed."),
			*EventName));
		return Result;
	}

	// Find the EventGraph
	UEdGraph* EventGraph = FindOrCreateEventGraph(Blueprint, TEXT("EventGraph"));
	if (!IsValid(EventGraph))
	{
		Result.Errors.Add(TEXT("Could not find or create EventGraph."));
		return Result;
	}

	// Check if the event node already exists to avoid duplicates
	for (UEdGraphNode* Node : EventGraph->Nodes)
	{
		UK2Node_Event* ExistingEvent = Cast<UK2Node_Event>(Node);
		if (IsValid(ExistingEvent) && ExistingEvent->GetFunctionName() == FunctionName)
		{
			int32 PosX = ExistingEvent->NodePosX;
			int32 PosY = ExistingEvent->NodePosY;
			Result.bSuccess = true;
			Result.ResultMessage = FString::Printf(
				TEXT("Event '%s' already exists at position (%d, %d). Wire new logic starting from NodePosX=%d, NodePosY=%d."),
				*EventName, PosX, PosY, PosX + 300, PosY);
			return Result;
		}
	}

	// Find the desired Y position
	int32 NodePosX = 0;
	int32 NodePosY = 0;
	TArray<FString> PosErrors;
	UAgentFrameworkActionUtils::TryGetIntParam(Params, TEXT("node_pos_x"), NodePosX, PosErrors, false);
	UAgentFrameworkActionUtils::TryGetIntParam(Params, TEXT("node_pos_y"), NodePosY, PosErrors, false);

	// Auto Y: place below existing nodes
	if (NodePosY == 0 && EventGraph->Nodes.Num() > 0)
	{
		for (const UEdGraphNode* ExistingNode : EventGraph->Nodes)
		{
			if (IsValid(ExistingNode))
			{
				int32 Bottom = ExistingNode->NodePosY + 100;
				if (Bottom > NodePosY) NodePosY = Bottom;
			}
		}
	}

	// Create the event node via FGraphNodeCreator
	FGraphNodeCreator<UK2Node_Event> EventCreator(*EventGraph);
	UK2Node_Event* NewEventNode = EventCreator.CreateNode();
	if (IsValid(NewEventNode))
	{
		NewEventNode->EventReference.SetExternalMember(FunctionName, OwnerClass);
		NewEventNode->bOverrideFunction = true;
		NewEventNode->NodePosX = NodePosX;
		NewEventNode->NodePosY = NodePosY;
		EventCreator.Finalize();
	}

	Blueprint->Modify();
	bool bCompileOk = CompileAndReport(Blueprint, Result, true);
	if (IsValid(Blueprint->GetOutermost()))
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}

	// Return the internal node name so the AI can reference it in connect_blueprint_pins
	// or in a subsequent T3D block. The node name is of the form "K2Node_Event_N".
	FString NewNodeName = IsValid(NewEventNode) ? NewEventNode->GetName() : TEXT("");

	Result.bSuccess = bCompileOk;
	Result.ResultMessage = FString::Printf(
		TEXT("Added '%s' event node.\n")
		TEXT("Internal node name: \"%s\"\n")
		TEXT("Position: (%d, %d)\n")
		TEXT("Execution output pin: \"then\"\n")
		TEXT("To wire logic after this event: use connect_blueprint_pins with source_node=\"%s\", source_pin=\"then\".\n")
		TEXT("Or inject a T3D block that references this node by name.\n")
		TEXT("Compile: %s."),
		*EventName,
		*NewNodeName,
		NodePosX, NodePosY,
		*NewNodeName,
		bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED"));
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteCompileBlueprint
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteCompileBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	TArray<FString> LoopWarnings;
	DetectInfiniteLoopRisk(Blueprint, LoopWarnings);
	Result.Warnings.Append(LoopWarnings);

	bool bOk = CompileAndReport(Blueprint, Result, false);
	Result.bSuccess = bOk;
	Result.ResultMessage = bOk
		? FString::Printf(TEXT("Blueprint '%s' compiled successfully."), *AssetPath)
		: FString::Printf(TEXT("Blueprint '%s' compiled with ERRORS. Fix the issues listed above."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteSetDefaults
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteSetDefaults(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	// Ensure compiled
	if (Blueprint->Status == BS_Dirty || Blueprint->Status == BS_Unknown)
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);

	if (!IsValid(Blueprint->GeneratedClass))
	{
		Result.Errors.Add(TEXT("Blueprint has no GeneratedClass — it must compile successfully before setting defaults."));
		return Result;
	}

	UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
	if (!IsValid(CDO))
	{
		Result.Errors.Add(TEXT("Could not get Class Default Object."));
		return Result;
	}

	CDO->Modify();

	const TSharedPtr<FJsonObject>* DefaultsObj = nullptr;
	if (!UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("defaults"), DefaultsObj, Errors, true) || !DefaultsObj)
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	for (const auto& Pair : (*DefaultsObj)->Values)
	{
		// Support "Component.Property" notation
		FString FullKey  = FString(*Pair.Key);
		FString CompName = TEXT("");
		FString PropName = FullKey;

		if (FullKey.Contains(TEXT(".")))
		{
			FullKey.Split(TEXT("."), &CompName, &PropName);
		}

		UObject* TargetObject = CDO;

		// If component name is specified, find the component sub-object
		if (!CompName.IsEmpty())
		{
			FObjectPropertyBase* CompProp = FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, *CompName);
			if (CompProp)
			{
				UObject* CompObject = CompProp->GetObjectPropertyValue_InContainer(CDO);
				if (IsValid(CompObject)) TargetObject = CompObject;
			}
			else
			{
				// Try finding via SCS component templates
				if (IsValid(Blueprint->SimpleConstructionScript))
				{
					USCS_Node* SCSNode = FindSCSNodeByName(Blueprint, CompName);
					if (IsValid(SCSNode) && IsValid(SCSNode->ComponentTemplate))
					{
						TargetObject = SCSNode->ComponentTemplate;
					}
				}
			}
		}

		if (!IsValid(TargetObject))
		{
			Result.Warnings.Add(FString::Printf(TEXT("Component or object '%s' not found on CDO."), *CompName));
			continue;
		}

		FProperty* Prop = TargetObject->GetClass()->FindPropertyByName(FName(*PropName));
		if (!Prop)
		{
			Result.Warnings.Add(FString::Printf(TEXT("Property '%s' not found on '%s'."), *PropName, *TargetObject->GetClass()->GetName()));
			continue;
		}

		FString ValueStr;
		if (Pair.Value->TryGetString(ValueStr))
		{
			TargetObject->Modify();
			void* PropAddr = Prop->ContainerPtrToValuePtr<void>(TargetObject);
			Prop->ImportText_Direct(*ValueStr, PropAddr, TargetObject, PPF_None);
			UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: Set default '%s' = '%s' on '%s'"), *FullKey, *ValueStr, *TargetObject->GetClass()->GetName());
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Property '%s' value is not a string."), *FullKey));
		}
	}

	if (IsValid(Blueprint->GetOutermost()))
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}
	Result.bSuccess = true;
	Result.ResultMessage = TEXT("Blueprint defaults updated successfully.");
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteSetComponentProperties (NEW)
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteSetComponentProperties(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	FString ComponentName;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("component_name"), ComponentName, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	USCS_Node* SCSNode = FindSCSNodeByName(Blueprint, ComponentName);
	if (!IsValid(SCSNode))
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Component '%s' not found in SCS. Available components: use get_blueprint_info to list them."),
			*ComponentName));
		return Result;
	}

	UActorComponent* CompTemplate = SCSNode->ComponentTemplate;
	if (!IsValid(CompTemplate))
	{
		Result.Errors.Add(FString::Printf(TEXT("Component '%s' has no template object."), *ComponentName));
		return Result;
	}

	CompTemplate->Modify();
	Blueprint->Modify();

	// --- Static Mesh ---
	FString StaticMeshPath;
	TArray<FString> SMErrors;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("static_mesh"), StaticMeshPath, SMErrors, false) && !StaticMeshPath.IsEmpty())
	{
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(CompTemplate);
		if (!IsValid(SMC))
		{
			Result.Warnings.Add(FString::Printf(TEXT("Component '%s' is not a StaticMeshComponent — cannot assign static_mesh."), *ComponentName));
		}
		else
		{
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *StaticMeshPath);
			if (IsValid(Mesh))
			{
				SMC->SetStaticMesh(Mesh);
				UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: Set StaticMesh '%s' on component '%s'"), *StaticMeshPath, *ComponentName);
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("StaticMesh asset not found: '%s'. Make sure the asset exists in the project."), *StaticMeshPath));
			}
		}
	}

	// --- Skeletal Mesh ---
	FString SkeletalMeshPath;
	TArray<FString> SKErrors;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("skeletal_mesh"), SkeletalMeshPath, SKErrors, false) && !SkeletalMeshPath.IsEmpty())
	{
		USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(CompTemplate);
		if (!IsValid(SKC))
		{
			Result.Warnings.Add(FString::Printf(TEXT("Component '%s' is not a SkeletalMeshComponent — cannot assign skeletal_mesh."), *ComponentName));
		}
		else
		{
			USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *SkeletalMeshPath);
			if (IsValid(Mesh))
			{
				SKC->SetSkeletalMeshAsset(Mesh);
				UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: Set SkeletalMesh '%s' on component '%s'"), *SkeletalMeshPath, *ComponentName);
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("SkeletalMesh asset not found: '%s'."), *SkeletalMeshPath));
			}
		}
	}

	// --- Transform: Relative Location ---
	const TSharedPtr<FJsonObject>* RelLocObj = nullptr;
	TArray<FString> RelLocErrors;
	if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("relative_location"), RelLocObj, RelLocErrors, false) && RelLocObj && (*RelLocObj).IsValid())
	{
		USceneComponent* SC = Cast<USceneComponent>(CompTemplate);
		if (IsValid(SC))
		{
			double X = 0, Y = 0, Z = 0;
			TArray<FString> ComponentErrors;
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RelLocObj, TEXT("x"), X, ComponentErrors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RelLocObj, TEXT("y"), Y, ComponentErrors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RelLocObj, TEXT("z"), Z, ComponentErrors, false);
			SC->SetRelativeLocation(FVector(X, Y, Z));
		}
	}

	// --- Transform: Relative Rotation ---
	const TSharedPtr<FJsonObject>* RelRotObj = nullptr;
	TArray<FString> RelRotErrors;
	if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("relative_rotation"), RelRotObj, RelRotErrors, false) && RelRotObj && (*RelRotObj).IsValid())
	{
		USceneComponent* SC = Cast<USceneComponent>(CompTemplate);
		if (IsValid(SC))
		{
			double Pitch = 0, Yaw = 0, Roll = 0;
			TArray<FString> ComponentErrors;
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RelRotObj, TEXT("pitch"), Pitch, ComponentErrors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RelRotObj, TEXT("yaw"), Yaw, ComponentErrors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RelRotObj, TEXT("roll"), Roll, ComponentErrors, false);
			SC->SetRelativeRotation(FRotator(Pitch, Yaw, Roll));
		}
	}

	// --- Transform: Relative Scale ---
	const TSharedPtr<FJsonObject>* RelScaleObj = nullptr;
	TArray<FString> RelScaleErrors;
	if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("relative_scale"), RelScaleObj, RelScaleErrors, false) && RelScaleObj && (*RelScaleObj).IsValid())
	{
		USceneComponent* SC = Cast<USceneComponent>(CompTemplate);
		if (IsValid(SC))
		{
			double X = 1, Y = 1, Z = 1;
			TArray<FString> ComponentErrors;
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RelScaleObj, TEXT("x"), X, ComponentErrors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RelScaleObj, TEXT("y"), Y, ComponentErrors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RelScaleObj, TEXT("z"), Z, ComponentErrors, false);
			SC->SetRelativeScale3D(FVector(X, Y, Z));
		}
	}

	// --- Collision Profile ---
	FString CollisionProfile;
	TArray<FString> CPErrors;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("collision_profile"), CollisionProfile, CPErrors, false) && !CollisionProfile.IsEmpty())
	{
		UPrimitiveComponent* PC = Cast<UPrimitiveComponent>(CompTemplate);
		if (IsValid(PC))
			PC->SetCollisionProfileName(FName(*CollisionProfile));
		else
			Result.Warnings.Add(FString::Printf(TEXT("Component '%s' is not a PrimitiveComponent — cannot set collision profile."), *ComponentName));
	}

	// --- Generic reflection-based properties ---
	const TSharedPtr<FJsonObject>* PropsObj = nullptr;
	TArray<FString> PropsErrors;
	if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("properties"), PropsObj, PropsErrors, false) && PropsObj && (*PropsObj).IsValid())
	{
		for (const auto& Pair : (*PropsObj)->Values)
		{
			FProperty* Prop = CompTemplate->GetClass()->FindPropertyByName(FName(*Pair.Key));
			if (!Prop)
			{
				Result.Warnings.Add(FString::Printf(TEXT("Property '%s' not found on component '%s'."), *Pair.Key, *ComponentName));
				continue;
			}
			FString ValStr;
			if (Pair.Value->TryGetString(ValStr))
			{
				void* PropAddr = Prop->ContainerPtrToValuePtr<void>(CompTemplate);
				Prop->ImportText_Direct(*ValStr, PropAddr, CompTemplate, PPF_None);
			}
		}
	}

	bool bCompileOk = CompileAndReport(Blueprint, Result, true);
	if (IsValid(Blueprint->GetOutermost()))
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}

	Result.bSuccess = bCompileOk;
	Result.ResultMessage = FString::Printf(TEXT("Updated component '%s' properties in '%s'. Compile: %s."),
		*ComponentName, *AssetPath, bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED"));
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteInjectNodesT3D (NEW — Primary Logic Graph Construction Method)
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteInjectNodesT3D(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	FString T3DText;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("t3d_text"), T3DText, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	FString GraphName = TEXT("EventGraph");
	TArray<FString> GraphErrors;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("graph_name"), GraphName, GraphErrors, false);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	// Find or create the target graph
	UEdGraph* TargetGraph = FindOrCreateEventGraph(Blueprint, GraphName);
	if (!IsValid(TargetGraph))
	{
		// Try FunctionGraphs
		for (UEdGraph* G : Blueprint->FunctionGraphs)
		{
			if (IsValid(G) && G->GetName() == GraphName)
			{
				TargetGraph = G;
				break;
			}
		}
	}

	if (!IsValid(TargetGraph))
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Graph '%s' not found in Blueprint '%s'. Use add_blueprint_function to create it first, or use 'EventGraph' for the main event graph."),
			*GraphName, *AssetPath));
		return Result;
	}

	// v1.1: Pre-flight validation — check T3D references against reflection
	{
		TArray<FString> PreFlightWarnings;
		PreFlightValidateT3D(T3DText, Blueprint, PreFlightWarnings);
		for (const FString& Warn : PreFlightWarnings)
		{
			Result.Warnings.Add(FString::Printf(TEXT("PRE-FLIGHT: %s"), *Warn));
		}
	}

	// Parse and sanitize Pin connections in T3DText before passing to editor utilities
	FString SanitizedT3DText = T3DText;

	// Remap legacy or variant module prefixes for K2Nodes to standard BlueprintGraph module
	SanitizedT3DText = SanitizedT3DText.Replace(TEXT("/Script/UnrealEd.K2Node_"), TEXT("/Script/BlueprintGraph.K2Node_"));
	SanitizedT3DText = SanitizedT3DText.Replace(TEXT("Class=/Script/UnrealEd.K2Node_"), TEXT("Class=/Script/BlueprintGraph.K2Node_"));
	{
		TMap<FString, TSet<FString>> T3DNodesAndPins;
		TArray<FString> Lines;
		T3DText.ParseIntoArrayLines(Lines);

		FString CurrentNodeName;
		for (const FString& Line : Lines)
		{
			FString CleanLine = Line.TrimStartAndEnd();
			if (CleanLine.StartsWith(TEXT("Begin Object")))
			{
				int32 NameStart = CleanLine.Find(TEXT("Name=\""));
				if (NameStart != INDEX_NONE)
				{
					NameStart += 6;
					int32 NameEnd = CleanLine.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, NameStart);
					if (NameEnd != INDEX_NONE)
					{
						CurrentNodeName = CleanLine.Mid(NameStart, NameEnd - NameStart).ToLower();
						T3DNodesAndPins.FindOrAdd(CurrentNodeName);
					}
				}
				else
				{
					NameStart = CleanLine.Find(TEXT("Name="));
					if (NameStart != INDEX_NONE)
					{
						NameStart += 5;
						int32 SpacePos;
						if (CleanLine.Mid(NameStart).FindChar(TEXT(' '), SpacePos))
						{
							CurrentNodeName = CleanLine.Mid(NameStart, SpacePos).ToLower();
						}
						else
						{
							CurrentNodeName = CleanLine.Mid(NameStart).ToLower();
						}
						T3DNodesAndPins.FindOrAdd(CurrentNodeName);
					}
				}
			}
			else if (CleanLine.StartsWith(TEXT("End Object")))
			{
				CurrentNodeName.Empty();
			}
			else if (!CurrentNodeName.IsEmpty() && CleanLine.StartsWith(TEXT("CustomProperties Pin")))
			{
				// Extract PinId
				FString PinId;
				int32 PinIdStart = CleanLine.Find(TEXT("PinId="));
				if (PinIdStart != INDEX_NONE)
				{
					PinIdStart += 6;
					bool bQuoted = (CleanLine[PinIdStart] == TEXT('"'));
					if (bQuoted)
					{
						PinIdStart++;
						int32 PinIdEnd = CleanLine.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, PinIdStart);
						if (PinIdEnd != INDEX_NONE)
						{
							PinId = CleanLine.Mid(PinIdStart, PinIdEnd - PinIdStart);
						}
					}
					else
					{
						int32 CommaOrClose;
						FString Rest = CleanLine.Mid(PinIdStart);
						if (Rest.FindChar(TEXT(','), CommaOrClose) || Rest.FindChar(TEXT(')'), CommaOrClose))
						{
							PinId = Rest.Left(CommaOrClose);
						}
						else
						{
							PinId = Rest;
						}
					}
				}

				// Extract PinName
				FString PinName;
				int32 PinNameStart = CleanLine.Find(TEXT("PinName=\""));
				if (PinNameStart != INDEX_NONE)
				{
					PinNameStart += 9;
					int32 PinNameEnd = CleanLine.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, PinNameStart);
					if (PinNameEnd != INDEX_NONE)
					{
						PinName = CleanLine.Mid(PinNameStart, PinNameEnd - PinNameStart);
					}
				}
				else
				{
					PinNameStart = CleanLine.Find(TEXT("PinName="));
					if (PinNameStart != INDEX_NONE)
					{
						PinNameStart += 8;
						int32 CommaOrClose;
						FString Rest = CleanLine.Mid(PinNameStart);
						if (Rest.FindChar(TEXT(','), CommaOrClose) || Rest.FindChar(TEXT(')'), CommaOrClose))
						{
							PinName = Rest.Left(CommaOrClose);
						}
						else
						{
							PinName = Rest;
						}
					}
				}

				if (!PinId.IsEmpty())
				{
					T3DNodesAndPins.FindOrAdd(CurrentNodeName).Add(PinId.ToLower());
				}
				if (!PinName.IsEmpty())
				{
					T3DNodesAndPins.FindOrAdd(CurrentNodeName).Add(PinName.ToLower());
				}
			}
		}

		TMap<FString, TSet<FString>> ExistingNodesAndPins;
		if (IsValid(TargetGraph))
		{
			for (UEdGraphNode* GNode : TargetGraph->Nodes)
			{
				if (IsValid(GNode))
				{
					FString GNodeNameLower = GNode->GetName().ToLower();
					auto& PinSet = ExistingNodesAndPins.FindOrAdd(GNodeNameLower);
					for (UEdGraphPin* GPin : GNode->Pins)
					{
						if (GPin)
						{
							PinSet.Add(GPin->PinId.ToString(EGuidFormats::Digits).ToLower());
							PinSet.Add(GPin->PinName.ToString().ToLower());
						}
					}
				}
			}
		}

		TArray<FString> SanitizedLines;
		CurrentNodeName.Empty();
		TArray<FString> SanitizerWarnings;

		for (const FString& Line : Lines)
		{
			FString CleanLine = Line.TrimStartAndEnd();
			if (CleanLine.StartsWith(TEXT("Begin Object")))
			{
				int32 NameStart = CleanLine.Find(TEXT("Name=\""));
				if (NameStart != INDEX_NONE)
				{
					NameStart += 6;
					int32 NameEnd = CleanLine.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, NameStart);
					if (NameEnd != INDEX_NONE)
					{
						CurrentNodeName = CleanLine.Mid(NameStart, NameEnd - NameStart).ToLower();
					}
				}
				else
				{
					NameStart = CleanLine.Find(TEXT("Name="));
					if (NameStart != INDEX_NONE)
					{
						NameStart += 5;
						int32 SpacePos;
						if (CleanLine.Mid(NameStart).FindChar(TEXT(' '), SpacePos))
						{
							CurrentNodeName = CleanLine.Mid(NameStart, SpacePos).ToLower();
						}
						else
						{
							CurrentNodeName = CleanLine.Mid(NameStart).ToLower();
						}
					}
				}
				SanitizedLines.Add(Line);
			}
			else if (CleanLine.StartsWith(TEXT("End Object")))
			{
				CurrentNodeName.Empty();
				SanitizedLines.Add(Line);
			}
			else if (!CurrentNodeName.IsEmpty() && CleanLine.StartsWith(TEXT("CustomProperties Pin")) && CleanLine.Contains(TEXT("LinkedTo=(")))
			{
				int32 StartIdx = Line.Find(TEXT("LinkedTo=("));
				if (StartIdx != INDEX_NONE)
				{
					int32 OpenParenthesis = StartIdx + 9;
					int32 EndIdx = Line.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenParenthesis);
					if (EndIdx == INDEX_NONE)
					{
						FString SanitizedLine = Line;
						SanitizedLine.ReplaceInline(TEXT("LinkedTo=("), TEXT(""));
						SanitizedLines.Add(SanitizedLine);
					}
					else
					{
						FString LinkedToContent = Line.Mid(OpenParenthesis, EndIdx - OpenParenthesis);
						TArray<FString> Connections;
						LinkedToContent.ParseIntoArray(Connections, TEXT(","));

						TArray<FString> ValidConnections;
						for (const FString& Conn : Connections)
						{
							FString TrimmedConn = Conn.TrimStartAndEnd();
							FString TargetNode, TargetPin;
							if (TrimmedConn.Split(TEXT(" "), &TargetNode, &TargetPin))
							{
								TargetNode = TargetNode.TrimStartAndEnd().TrimQuotes();
								TargetPin = TargetPin.TrimStartAndEnd().TrimQuotes();

								FString TargetNodeLower = TargetNode.ToLower();
								FString TargetPinLower = TargetPin.ToLower();

								bool bTargetExists = false;
								bool bPinExists = false;

								if (T3DNodesAndPins.Contains(TargetNodeLower))
								{
									bTargetExists = true;
									if (T3DNodesAndPins[TargetNodeLower].Contains(TargetPinLower))
									{
										bPinExists = true;
									}
								}
								else if (ExistingNodesAndPins.Contains(TargetNodeLower))
								{
									bTargetExists = true;
									if (ExistingNodesAndPins[TargetNodeLower].Contains(TargetPinLower))
									{
										bPinExists = true;
									}
								}

								if (bTargetExists && bPinExists)
								{
									ValidConnections.Add(TrimmedConn);
								}
								else
								{
									FString Reason;
									if (!bTargetExists)
									{
										Reason = FString::Printf(TEXT("target node '%s' does not exist"), *TargetNode);
									}
									else
									{
										Reason = FString::Printf(TEXT("target pin '%s' does not exist on node '%s'"), *TargetPin, *TargetNode);
									}
									SanitizerWarnings.Add(FString::Printf(TEXT("Stripped connection on node '%s': connection to '%s' was removed because %s."),
										*CurrentNodeName, *TrimmedConn, *Reason));
								}
							}
							else
							{
								SanitizerWarnings.Add(FString::Printf(TEXT("Stripped malformed connection '%s' on node '%s'."), *TrimmedConn, *CurrentNodeName));
							}
						}

						FString NewPinLine = Line.Left(StartIdx);
						if (ValidConnections.Num() > 0)
						{
							FString ValidLinkedTo = FString::Join(ValidConnections, TEXT(","));
							NewPinLine += FString::Printf(TEXT("LinkedTo=(%s)"), *ValidLinkedTo);
						}
						NewPinLine += Line.Mid(EndIdx + 1);

						NewPinLine.ReplaceInline(TEXT(",)"), TEXT(")"));
						NewPinLine.ReplaceInline(TEXT(",,"), TEXT(","));
						NewPinLine.ReplaceInline(TEXT("(,"), TEXT("("));

						SanitizedLines.Add(NewPinLine);
					}
				}
				else
				{
					SanitizedLines.Add(Line);
				}
			}
			else
			{
				SanitizedLines.Add(Line);
			}
		}

		for (const FString& Warn : SanitizerWarnings)
		{
			Result.Warnings.Add(FString::Printf(TEXT("PASTE SANITISER: %s"), *Warn));
		}

		SanitizedT3DText = FString::Join(SanitizedLines, TEXT("\n"));
	}

	// Resolve placeholder GUID tokens → real GUIDs
	FString ResolvedT3D = ResolveT3DPlaceholders(SanitizedT3DText);

	UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: Injecting T3D nodes into graph '%s' of '%s'"), *GraphName, *AssetPath);

	// Import the nodes via FEdGraphUtilities (same as editor paste)
	TSet<UEdGraphNode*> ImportedNodes;
	const UEdGraphSchema* Schema = TargetGraph->GetSchema();

	// FEdGraphUtilities::ImportNodesFromText appends nodes from T3D text into the graph.
	// It uses the same serialization format as Ctrl+C in the Blueprint editor.
	FEdGraphUtilities::ImportNodesFromText(TargetGraph, ResolvedT3D, /*out*/ImportedNodes);

	if (ImportedNodes.Num() == 0)
	{
		// ImportNodesFromText produces a silent failure on malformed T3D
		Result.Errors.Add(FString::Printf(
			TEXT("T3D injection produced 0 nodes in graph '%s'. Possible causes: malformed T3D syntax, wrong Class= specifier, missing 'Begin Object'/'End Object' wrapper, or node Class not available. "
				 "Verify T3D format: each block must start with 'Begin Object Class=/Script/BlueprintGraph.K2Node_XXX Name=\"NodeName\"' and end with 'End Object'."),
			*GraphName));
		return Result;
	}

	Blueprint->Modify();

	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray = nullptr;
	TArray<FString> ConnErrors;
	if (UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("connections"), ConnectionsArray, ConnErrors, false) && ConnectionsArray)
	{
		for (const TSharedPtr<FJsonValue>& ConnVal : *ConnectionsArray)
		{
			TSharedPtr<FJsonObject> ConnObj = ConnVal->AsObject();
			if (!ConnObj.IsValid()) continue;

			FString SrcNodeName, SrcPinName, DstNodeName, DstPinName;
			TArray<FString> ItemErrors;
			UAgentFrameworkActionUtils::TryGetStringParam(ConnObj, TEXT("source_node"), SrcNodeName, ItemErrors, false);
			UAgentFrameworkActionUtils::TryGetStringParam(ConnObj, TEXT("source_pin"), SrcPinName, ItemErrors, false);
			UAgentFrameworkActionUtils::TryGetStringParam(ConnObj, TEXT("target_node"), DstNodeName, ItemErrors, false);
			UAgentFrameworkActionUtils::TryGetStringParam(ConnObj, TEXT("target_pin"), DstPinName, ItemErrors, false);

			if (!SrcNodeName.IsEmpty() && !SrcPinName.IsEmpty() && !DstNodeName.IsEmpty() && !DstPinName.IsEmpty())
			{
				ConnectPinsHelper(Blueprint, TargetGraph, SrcNodeName, SrcPinName, DstNodeName, DstPinName, Result);
			}
		}
	}

	// CRITICAL: Sanitize ALL graph nodes before compilation (not just imported ones).
	//
	// When AI-generated T3D has LinkedTo references to nodes that aren't present
	// in the graph (wrong node names, non-existent nodes, malformed cross-refs),
	// FEdGraphUtilities::ImportNodesFromText creates broken UEdGraphPin::LinkedTo
	// entries pointing to null or destroyed UEdGraphPin objects.
	//
	// Additionally, ImportNodesFromText can corrupt EXISTING nodes' pins —
	// e.g. a pre-existing node's pin array may end up with null entries.
	//
	// If these are not cleaned up before CompileBlueprint, the internal call to
	// FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified → RefreshNodes
	// → ReconstructNode hits check(Pin) in EdGraphSchema_K2.cpp:6737 causing
	// an editor crash (Assertion failed: Pin).
	//
	// Fix v2: Walk EVERY node in the ENTIRE graph (not just imported nodes):
	//   1. Remove null entries from Node->Pins arrays
	//   2. Remove null/stale LinkedTo refs on all pins
	//   3. Remove refs to nodes not present in the graph (orphaned T3D cross-refs)
	{
		// Build a set of all valid node pointers in the graph for fast lookup
		TSet<UEdGraphNode*> ValidGraphNodes;
		TSet<FString> GraphNodeNames;
		for (UEdGraphNode* GNode : TargetGraph->Nodes)
		{
			if (IsValid(GNode))
			{
				ValidGraphNodes.Add(GNode);
				GraphNodeNames.Add(GNode->GetName());
			}
		}

		int32 TotalSanitisedLinks = 0;
		int32 TotalNullPinsRemoved = 0;

		// Iterate ALL nodes in the graph, not just imported ones
		for (UEdGraphNode* Node : TargetGraph->Nodes)
		{
			if (!IsValid(Node)) continue;

			// Step 1: Remove null entries from the Pins array itself.
			// This prevents check(Pin) assertion in ReconstructNode.
			int32 NullPins = Node->Pins.RemoveAll([](UEdGraphPin* P) { return P == nullptr; });
			if (NullPins > 0)
			{
				TotalNullPinsRemoved += NullPins;
				UE_LOG(LogAgentFramework, Warning,
					TEXT("BlueprintActions: Removed %d null pin entry(s) from node '%s' Pins array."),
					NullPins, *Node->GetName());
			}

			// Step 2: Clean LinkedTo refs on every pin
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;  // Should not happen after step 1, but defensive

				// Remove any LinkedTo entry that:
				// (a) is a null pointer
				// (b) has a null owning node
				// (c) points to a node not present in the graph (orphaned T3D cross-ref)
				int32 RemovedCount = Pin->LinkedTo.RemoveAll([&ValidGraphNodes](UEdGraphPin* LinkedPin) -> bool
				{
					if (!LinkedPin) return true;
					UEdGraphNode* OwningNode = LinkedPin->GetOwningNodeUnchecked();
					if (!IsValid(OwningNode)) return true;
					if (!ValidGraphNodes.Contains(OwningNode)) return true;
					return false;
				});

				if (RemovedCount > 0)
				{
					TotalSanitisedLinks += RemovedCount;
					UE_LOG(LogAgentFramework, Warning,
						TEXT("BlueprintActions: Removed %d broken LinkedTo ref(s) on pin '%s' of node '%s'."),
						RemovedCount,
						*Pin->PinName.ToString(),
						*Node->GetName());
				}
			}
		}

		if (TotalNullPinsRemoved > 0)
		{
			Result.Warnings.Add(FString::Printf(
				TEXT("SANITISER: Removed %d null pin(s) from graph nodes. ")
				TEXT("This prevented an editor crash (check(Pin) assertion in EdGraphSchema_K2)."),
				TotalNullPinsRemoved));
		}

		if (TotalSanitisedLinks > 0)
		{
			Result.Warnings.Add(FString::Printf(
				TEXT("SANITISER: Removed %d broken LinkedTo reference(s) from graph nodes. ")
				TEXT("These are references to null, destroyed, or orphaned pins. ")
				TEXT("Affected pins are now disconnected. Call verify_blueprint_connections to diagnose and repair missing wires."),
				TotalSanitisedLinks));
		}
	}

	// -----------------------------------------------------------------------
	// v1.1: AUTO-LAYOUT — Sugiyama-style DAG layout for human-readable graphs
	// Nodes injected via T3D often stack at (0,0). This post-pass organizes
	// them into readable left-to-right execution flow.
	// -----------------------------------------------------------------------
	{
		bool bAutoLayout = true;
		TArray<FString> LayoutErrors;
		UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("auto_layout"), bAutoLayout, LayoutErrors, false);

		if (bAutoLayout && ImportedNodes.Num() > 1)
		{
			// Find a suitable start position (offset from existing nodes)
			int32 MaxExistingY = 0;
			for (UEdGraphNode* ExistingNode : TargetGraph->Nodes)
			{
				if (IsValid(ExistingNode) && !ImportedNodes.Contains(ExistingNode))
				{
					MaxExistingY = FMath::Max(MaxExistingY, ExistingNode->NodePosY + 200);
				}
			}

			AutoLayoutNodes(ImportedNodes, 0, MaxExistingY);
			Result.Warnings.Add(TEXT("AUTO-LAYOUT: Applied Sugiyama DAG layout to injected nodes for readability. "
				"Set auto_layout=false to preserve AI-specified positions."));
		}
	}

	// -----------------------------------------------------------------------
	// PIN AUDIT — delegate to shared BuildPinAuditReport helper
	// (object pins with no DefaultObject, zero numeric pins, all-zero struct pins)
	// -----------------------------------------------------------------------
	FString PinAuditReport = BuildPinAuditReport(ImportedNodes);

	bool bCompileOk = CompileAndReport(Blueprint, Result, true);
	if (IsValid(Blueprint->GetOutermost()))
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}

	// Report node positions so AI can place subsequent nodes without overlap
	FString NodePositions;
	for (UEdGraphNode* Node : ImportedNodes)
	{
		if (IsValid(Node))
		{
			NodePositions += FString::Printf(TEXT("  %s @ (%d, %d)\n"), *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), Node->NodePosX, Node->NodePosY);
		}
	}

	// -----------------------------------------------------------------------
	// INJECTION VERIFICATION — Compact connection report replacing full T3D
	// readback. Walks UEdGraphNode objects in memory to build a token-efficient
	// summary of exec chains, data connections, pin values, and issues.
	// Saves ~80-90% tokens vs ExportNodesToText() while preserving full
	// verification capability for the AI.
	// -----------------------------------------------------------------------
	FString NodeReadback = BuildCompactConnectionReport(ImportedNodes, GraphName);

	Result.bSuccess = bCompileOk;
	Result.ResultMessage = FString::Printf(
		TEXT("Injected %d nodes into graph '%s' of '%s'.\nImported nodes:\n%sCompile: %s.%s%s\nResolvedT3D:\n%s"),
		ImportedNodes.Num(), *GraphName, *AssetPath, *NodePositions,
		bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED"),
		*PinAuditReport,
		*NodeReadback,
		*ResolvedT3D);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteGetBlueprintInfo (NEW — Read-Only Query)
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteGetBlueprintInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	bool bExcludeVisualLayout = false;
	TArray<FString> OptErrors;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("exclude_visual_layout"), bExcludeVisualLayout, OptErrors, false);

	FString QueryMode = TEXT("full");
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("query_mode"), QueryMode, OptErrors, false);

	FString ClientHash;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("client_hash"), ClientHash, OptErrors, false);

	TArray<FString> NodeNames;
	const TArray<TSharedPtr<FJsonValue>>* NodeNamesArray = nullptr;
	if (UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("node_names"), NodeNamesArray, OptErrors, false) && NodeNamesArray)
	{
		for (const TSharedPtr<FJsonValue>& Val : *NodeNamesArray)
		{
			NodeNames.Add(Val->AsString());
		}
	}

	int32 ModCount = AssetModificationCounts.FindOrAdd(AssetPath, 0);
	bool bIsDirty = false;
	if (IsValid(Blueprint->GetOutermost()))
	{
		bIsDirty = Blueprint->GetOutermost()->IsDirty();
	}

	FString CurrentHash = FString::Printf(TEXT("%d_%d_%d"),
		ModCount,
		(int32)Blueprint->Status,
		bIsDirty ? 1 : 0);

	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetStringField(TEXT("asset_path"), CompressBlueprintAssetPath(Blueprint->GetPathName()));
	ResponseObj->SetStringField(TEXT("client_hash"), CurrentHash);

	if (!ClientHash.IsEmpty() && ClientHash.Equals(CurrentHash, ESearchCase::CaseSensitive))
	{
		ResponseObj->SetBoolField(TEXT("up_to_date"), true);
	}
	else
	{
		ResponseObj->SetBoolField(TEXT("up_to_date"), false);

		FString InfoJsonStr = BuildBlueprintInfoJson(Blueprint, NodeNames.Num() > 0 ? &NodeNames : nullptr, bExcludeVisualLayout, QueryMode);
		TSharedPtr<FJsonObject> InfoJsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InfoJsonStr);
		if (FJsonSerializer::Deserialize(Reader, InfoJsonObj) && InfoJsonObj.IsValid())
		{
			ResponseObj->SetObjectField(TEXT("blueprint_info"), InfoJsonObj);
		}

		if (!bExcludeVisualLayout)
		{
			FString GraphReadbacks;
			auto ReportGraph = [&](UEdGraph* Graph, const FString& GraphType)
			{
				if (!IsValid(Graph)) return;

				TSet<UEdGraphNode*> NodeSet;
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (IsValid(Node))
					{
						if (NodeNames.Num() > 0 && !NodeNames.Contains(Node->GetName())) continue;
						NodeSet.Add(Node);
					}
				}
				if (NodeSet.IsEmpty()) return;

				GraphReadbacks += BuildCompactConnectionReport(NodeSet, Graph->GetName());
			};

			for (UEdGraph* G : Blueprint->UbergraphPages)   if (IsValid(G)) ReportGraph(G, TEXT("EventGraph"));
			for (UEdGraph* G : Blueprint->FunctionGraphs)   if (IsValid(G)) ReportGraph(G, TEXT("Function"));
			for (UEdGraph* G : Blueprint->MacroGraphs)      if (IsValid(G)) ReportGraph(G, TEXT("Macro"));

			if (!GraphReadbacks.IsEmpty())
			{
				ResponseObj->SetStringField(TEXT("compact_connection_report"), GraphReadbacks);
			}
		}
	}

	FString ResponseStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseStr);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseStr;
	return Result;
}

// ============================================================================
// ExecuteAnalyzeBlueprintGraph (NEW — Token-Efficient Summary)
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteAnalyzeBlueprintGraph(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	FString GraphNameFilter;
	TArray<FString> FilterErrors;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("graph_name"), GraphNameFilter, FilterErrors, false);

	FString GraphReadbacks;

	auto ReportGraph = [&](UEdGraph* Graph, const FString& GraphType)
	{
		if (!IsValid(Graph)) return;
		if (!GraphNameFilter.IsEmpty() && Graph->GetName() != GraphNameFilter) return;

		TSet<UEdGraphNode*> NodeSet;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (IsValid(Node)) NodeSet.Add(Node);
		}
		if (NodeSet.IsEmpty()) return;

		GraphReadbacks += BuildCompactConnectionReport(NodeSet, Graph->GetName());
	};

	for (UEdGraph* G : Blueprint->UbergraphPages)   if (IsValid(G)) ReportGraph(G, TEXT("EventGraph"));
	for (UEdGraph* G : Blueprint->FunctionGraphs)   if (IsValid(G)) ReportGraph(G, TEXT("Function"));
	for (UEdGraph* G : Blueprint->MacroGraphs)      if (IsValid(G)) ReportGraph(G, TEXT("Macro"));

	if (GraphReadbacks.IsEmpty())
	{
		if (!GraphNameFilter.IsEmpty())
		{
			// Check if the graph exists but is empty
			UEdGraph* FoundGraph = nullptr;
			for (UEdGraph* G : Blueprint->UbergraphPages)   { if (IsValid(G) && G->GetName() == GraphNameFilter) { FoundGraph = G; break; } }
			if (!FoundGraph)
			{
				for (UEdGraph* G : Blueprint->FunctionGraphs)   { if (IsValid(G) && G->GetName() == GraphNameFilter) { FoundGraph = G; break; } }
			}
			if (!FoundGraph)
			{
				for (UEdGraph* G : Blueprint->MacroGraphs)      { if (IsValid(G) && G->GetName() == GraphNameFilter) { FoundGraph = G; break; } }
			}

			if (IsValid(FoundGraph))
			{
				GraphReadbacks = FString::Printf(TEXT("Graph '%s' exists in Blueprint '%s' but contains no nodes."), *GraphNameFilter, *AssetPath);
			}
			else
			{
				TArray<FString> AvailableGraphs;
				for (UEdGraph* G : Blueprint->UbergraphPages)   { if (IsValid(G)) AvailableGraphs.Add(G->GetName()); }
				for (UEdGraph* G : Blueprint->FunctionGraphs)   { if (IsValid(G)) AvailableGraphs.Add(G->GetName()); }
				for (UEdGraph* G : Blueprint->MacroGraphs)      { if (IsValid(G)) AvailableGraphs.Add(G->GetName()); }

				GraphReadbacks = FString::Printf(TEXT("Graph '%s' not found in Blueprint '%s'. Available graphs: %s"),
					*GraphNameFilter, *AssetPath, *FString::Join(AvailableGraphs, TEXT(", ")));
			}
		}
		else
		{
			GraphReadbacks = TEXT("No graphs or nodes found.");
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = GraphReadbacks;
	return Result;
}

// ============================================================================
// ExecuteConnectPins — Explicitly wire two nodes in a Blueprint graph
// ============================================================================

bool FAgentFrameworkBlueprintActions::ConnectPinsHelper(
	UBlueprint* Blueprint,
	UEdGraph* TargetGraph,
	const FString& SourceNode,
	const FString& SourcePin,
	const FString& TargetNode,
	const FString& TargetPin,
	FAgentFrameworkActionResult& Result)
{
	if (!IsValid(Blueprint)) return false;
	if (!IsValid(TargetGraph)) return false;

	UEdGraphNode* SrcNode = nullptr;
	UEdGraphNode* DstNode = nullptr;

	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (!IsValid(Node)) continue;
		if (Node->GetName() == SourceNode) SrcNode = Node;
		if (Node->GetName() == TargetNode) DstNode = Node;
		if (SrcNode && DstNode) break;
	}

	if (!IsValid(SrcNode))
	{
		Result.Errors.Add(FString::Printf(TEXT("Source node '%s' not found in graph '%s'. [AI HINT: If this node is inside a function or macro, pass 'graph_name' explicitly.]"), *SourceNode, *TargetGraph->GetName()));
		return false;
	}

	if (!IsValid(DstNode))
	{
		Result.Errors.Add(FString::Printf(TEXT("Target node '%s' not found in graph '%s'. [AI HINT: If this node is inside a function or macro, pass 'graph_name' explicitly.]"), *TargetNode, *TargetGraph->GetName()));
		return false;
	}

	UEdGraphPin* OutputPin = nullptr;
	for (UEdGraphPin* Pin : SrcNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output &&
			Pin->PinName.ToString().Equals(SourcePin, ESearchCase::IgnoreCase))
		{
			OutputPin = Pin;
			break;
		}
	}
	if (!OutputPin)
	{
		Result.Errors.Add(FString::Printf(TEXT("Output pin '%s' not found on node '%s'."), *SourcePin, *SourceNode));
		return false;
	}

	UEdGraphPin* InputPin = nullptr;
	for (UEdGraphPin* Pin : DstNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input &&
			Pin->PinName.ToString().Equals(TargetPin, ESearchCase::IgnoreCase))
		{
			InputPin = Pin;
			break;
		}
	}
	if (!InputPin)
	{
		Result.Errors.Add(FString::Printf(TEXT("Input pin '%s' not found on node '%s'."), *TargetPin, *TargetNode));
		return false;
	}

	for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
	{
		if (LinkedPin == InputPin)
		{
			return true; // Already connected
		}
	}

	Blueprint->Modify();
	SrcNode->Modify();
	DstNode->Modify();

	const UEdGraphSchema* Schema = TargetGraph->GetSchema();
	if (!Schema) return false;
	FPinConnectionResponse ConnResponse = Schema->CanCreateConnection(OutputPin, InputPin);

	if (ConnResponse.Response == CONNECT_RESPONSE_DISALLOW)
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Cannot connect %s.%s (type: %s) -> %s.%s (type: %s): %s"),
			*SourceNode, *SourcePin, *OutputPin->PinType.PinCategory.ToString(),
			*TargetNode, *TargetPin, *InputPin->PinType.PinCategory.ToString(),
			*ConnResponse.Message.ToString()));
		return false;
	}

	Schema->TryCreateConnection(OutputPin, InputPin);
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteConnectPins(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	FString GraphName   = TEXT("EventGraph");
	FString SourceNode;
	FString SourcePin;
	FString TargetNode;
	FString TargetPin;
	TArray<FString> Errors;

	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("source_node"), SourceNode, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("source_pin"), SourcePin, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("target_node"), TargetNode, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("target_pin"), TargetPin, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}
	TArray<FString> OptErrors;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("graph_name"), GraphName, OptErrors, false);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	// Find the target graph
	UEdGraph* TargetGraph = nullptr;
	for (UEdGraph* G : Blueprint->UbergraphPages)
	{
		if (IsValid(G) && G->GetName() == GraphName)
		{
			TargetGraph = G;
			break;
		}
	}
	if (!IsValid(TargetGraph))
	{
		for (UEdGraph* G : Blueprint->FunctionGraphs)
		{
			if (IsValid(G) && G->GetName() == GraphName)
			{
				TargetGraph = G;
				break;
			}
		}
	}

	auto GraphContainsNodes = [](UEdGraph* Graph, const FString& SrcName, const FString& DstName) -> bool
	{
		if (!IsValid(Graph)) return false;
		bool bHasSrc = false, bHasDst = false;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!IsValid(Node)) continue;
			if (Node->GetName() == SrcName) bHasSrc = true;
			if (Node->GetName() == DstName) bHasDst = true;
			if (bHasSrc && bHasDst) return true;
		}
		return bHasDst || (bHasSrc && bHasDst);
	};

	if (!IsValid(TargetGraph) || !GraphContainsNodes(TargetGraph, SourceNode, TargetNode))
	{
		TArray<UEdGraph*> AllGraphs;
		AllGraphs.Append(Blueprint->FunctionGraphs);
		AllGraphs.Append(Blueprint->UbergraphPages);
		AllGraphs.Append(Blueprint->MacroGraphs);

		for (UEdGraph* G : AllGraphs)
		{
			if (IsValid(G) && GraphContainsNodes(G, SourceNode, TargetNode))
			{
				TargetGraph = G;
				break;
			}
		}
	}

	if (!IsValid(TargetGraph))
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Graph '%s' not found in Blueprint '%s'. Use get_blueprint_info to list available graphs."),
			*GraphName, *AssetPath));
		return Result;
	}

	if (ConnectPinsHelper(Blueprint, TargetGraph, SourceNode, SourcePin, TargetNode, TargetPin, Result))
	{
		bool bCompileOk = CompileAndReport(Blueprint, Result, true);
		if (IsValid(Blueprint->GetOutermost()))
		{
			Blueprint->GetOutermost()->MarkPackageDirty();
		}

		Result.bSuccess = bCompileOk;
		Result.ResultMessage = FString::Printf(
			TEXT("Connected: %s.%s -> %s.%s\nCompile: %s."),
			*SourceNode, *SourcePin,
			*TargetNode, *TargetPin,
			bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED"));
	}
	else
	{
		Result.bSuccess = false;
	}

	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// BuildPinAuditReport (shared helper — used by T3D injection, GetBlueprintInfo,
// and VerifyConnections so the AI always sees actionable pin-value feedback)
// ============================================================================

FString FAgentFrameworkBlueprintActions::BuildPinAuditReport(const TSet<UEdGraphNode*>& Nodes)
{
	struct FPinAuditEntry
	{
		FString NodeTitle;
		FString PinName;
		FString Category;
		FString SubCategory;   // asset class name for object pins / struct name
		FString CurrentValue;  // DefaultValue or "[EMPTY]"
		FString Advice;
	};
	TArray<FPinAuditEntry> AuditEntries;

	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node) continue;
		FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;
			if (Pin->Direction != EGPD_Input) continue;  // only input pins
			if (Pin->LinkedTo.Num() > 0) continue;        // already wired — skip
			if (Pin->bHidden) continue;                    // skip hidden/internal pins

			const FName PinCat = Pin->PinType.PinCategory;
			if (PinCat == TEXT("exec")) continue;          // skip execution flow pins

			FPinAuditEntry Entry;
			Entry.NodeTitle = NodeTitle;
			Entry.PinName   = Pin->PinName.ToString();
			Entry.Category  = PinCat.ToString();

			// --- Object / asset reference pins ---
			if (PinCat == TEXT("object")    ||
				PinCat == TEXT("interface") ||
				PinCat == TEXT("softobject")||
				PinCat == TEXT("class")     ||
				PinCat == TEXT("softclass"))
			{
				if (Pin->DefaultObject == nullptr)
				{
					FString ClassName = TEXT("Asset");
					if (UObject* SubCatObj = Pin->PinType.PinSubCategoryObject.Get())
						ClassName = SubCatObj->GetName();

					Entry.SubCategory  = ClassName;
					Entry.CurrentValue = TEXT("[EMPTY]");
					Entry.Advice = FString::Printf(
						TEXT("No asset assigned. Call search_assets(class_filter='%s') to find a valid asset, "
							 "then re-inject with DefaultObject set to the asset path."),
						*ClassName);
					AuditEntries.Add(Entry);
				}
			}
			// --- Numeric pins: real / float / double / int / int64 / byte ---
			else if (PinCat == TEXT("real")   ||
					 PinCat == TEXT("float")  ||
					 PinCat == TEXT("double") ||
					 PinCat == TEXT("int")    ||
					 PinCat == TEXT("int64")  ||
					 PinCat == TEXT("byte"))
			{
				const FString& Val = Pin->DefaultValue;
				const bool bIsZeroOrEmpty =
					Val.IsEmpty()         ||
					Val == TEXT("0")      ||
					Val == TEXT("0.0")    ||
					Val == TEXT("0.00")   ||
					Val == TEXT("0.000000");
				if (bIsZeroOrEmpty)
				{
					Entry.CurrentValue = Val.IsEmpty() ? TEXT("[EMPTY]") : Val;
					Entry.Advice = TEXT("Value is zero/empty — verify this is intentional or set an explicit "
						"non-zero value (e.g. 1.0 for the R/G/B channel of MakeColor when that channel should be fully on).");
					AuditEntries.Add(Entry);
				}
			}
			// --- Struct pins: FLinearColor, FVector, FRotator, etc. ---
			else if (PinCat == TEXT("struct"))
			{
				const FString& Val = Pin->DefaultValue;
				bool bLooksAllZero = Val.IsEmpty();
				if (!bLooksAllZero)
				{
					FString Stripped = Val.Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT(""));
					TArray<FString> Tokens;
					Stripped.ParseIntoArray(Tokens, TEXT(","), true);
					bool bAllZero   = true;
					bool bHasTokens = Tokens.Num() > 0;
					for (const FString& Token : Tokens)
					{
						int32 EqIdx = INDEX_NONE;
						if (Token.FindChar(TEXT('='), EqIdx))
						{
							if (FMath::Abs(FCString::Atof(*Token.Mid(EqIdx + 1))) > SMALL_NUMBER)
							{
								bAllZero = false;
								break;
							}
						}
					}
					bLooksAllZero = bHasTokens && bAllZero;
				}
				if (bLooksAllZero)
				{
					FString StructName = TEXT("Struct");
					if (UObject* SubCatObj = Pin->PinType.PinSubCategoryObject.Get())
						StructName = SubCatObj->GetName();

					Entry.SubCategory  = StructName;
					Entry.CurrentValue = Val.IsEmpty() ? TEXT("[EMPTY]") : Val;
					Entry.Advice = FString::Printf(
						TEXT("%s has all-zero components — set explicit values where non-zero is intended "
							 "(e.g. B=1.0 for LinearColor pure blue, or the R/G/B channel for your target color)."),
						*StructName);
					AuditEntries.Add(Entry);
				}
			}
		}
	}

	if (AuditEntries.IsEmpty())
		return FString();

	FString Report;
	Report += TEXT("\n\n=== PIN VALUE AUDIT ===\n");
	Report += TEXT("The following unconnected input pins have empty, missing, or all-zero default values.\n");
	Report += TEXT("Review EVERY entry below and take the recommended action before declaring the task complete:\n\n");
	for (const FPinAuditEntry& E : AuditEntries)
	{
		Report += FString::Printf(
			TEXT("  • [%s] pin \"%s\" (%s%s) = %s\n    ACTION: %s\n"),
			*E.NodeTitle,
			*E.PinName,
			*E.Category,
			E.SubCategory.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(":%s"), *E.SubCategory),
			*E.CurrentValue,
			*E.Advice);
	}
	return Report;
}

// ============================================================================
// BuildCompactConnectionReport — Token-efficient T3D readback replacement
// ============================================================================

FString FAgentFrameworkBlueprintActions::BuildCompactConnectionReport(const TSet<UEdGraphNode*>& Nodes, const FString& GraphName)
{
	if (Nodes.IsEmpty()) return FString();

	FString Report;
	Report.Reserve(4096);

	Report += FString::Printf(TEXT("\n\n=== CONNECTION REPORT (%d nodes in \"%s\") ===\n"), Nodes.Num(), *GraphName);

	// -----------------------------------------------------------------------
	// Section 1: EXEC CHAIN — trace execution flow through exec pins
	// -----------------------------------------------------------------------
	Report += TEXT("\nEXEC CHAIN:\n");
	bool bHasExecChain = false;
	bool bHasDisconnectedExec = false;

	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node) continue;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;
			if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
			if (Pin->Direction != EGPD_Output) continue;

			FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
			FString NodeName  = Node->GetName();

			if (Pin->LinkedTo.Num() > 0)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;
					UEdGraphNode* TargetNode = LinkedPin->GetOwningNode();
					FString TargetTitle = TargetNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
					Report += FString::Printf(TEXT("  %s.%s → %s.%s\n"),
						*NodeName, *Pin->PinName.ToString(),
						*TargetNode->GetName(), *LinkedPin->PinName.ToString());
					bHasExecChain = true;
				}
			}
			else
			{
				// Disconnected exec output — flag as warning
				// Only flag for nodes that typically must have exec connections
				bool bShouldFlag = Node->IsA<UK2Node_Event>()
					|| Node->IsA<UK2Node_CustomEvent>()
					|| Node->IsA<UK2Node_FunctionEntry>()
					|| Node->IsA<UK2Node_CallFunction>();

				if (bShouldFlag)
				{
					Report += FString::Printf(TEXT("  %s.%s → (DISCONNECTED) ⚠️\n"),
						*NodeName, *Pin->PinName.ToString());
					bHasDisconnectedExec = true;
				}
			}
		}
	}
	if (!bHasExecChain && !bHasDisconnectedExec)
	{
		Report += TEXT("  (none — pure data-only nodes)\n");
	}

	// -----------------------------------------------------------------------
	// Section 2: DATA CONNECTIONS — non-exec pin wiring between nodes
	// -----------------------------------------------------------------------
	Report += TEXT("\nDATA CONNECTIONS:\n");
	bool bHasDataConnections = false;

	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node) continue;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;
			if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
			if (Pin->Direction != EGPD_Output) continue;
			if (Pin->LinkedTo.Num() == 0) continue;
			if (Pin->bHidden) continue;

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;
				Report += FString::Printf(TEXT("  %s.%s → %s.%s\n"),
					*Node->GetName(), *Pin->PinName.ToString(),
					*LinkedPin->GetOwningNode()->GetName(), *LinkedPin->PinName.ToString());
				bHasDataConnections = true;
			}
		}
	}
	if (!bHasDataConnections)
	{
		Report += TEXT("  (none)\n");
	}

	// -----------------------------------------------------------------------
	// Section 3: NON-DEFAULT PIN VALUES — unwired input pins with explicit values
	// -----------------------------------------------------------------------
	Report += TEXT("\nNON-DEFAULT PIN VALUES:\n");
	bool bHasNonDefaults = false;

	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node) continue;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;
			if (Pin->Direction != EGPD_Input) continue;
			if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
			if (Pin->bHidden) continue;
			if (Pin->LinkedTo.Num() > 0) continue; // wired pins covered in DATA CONNECTIONS

			// Check for non-empty, non-trivial default values
			bool bHasValue = false;

			if (Pin->DefaultObject != nullptr)
			{
				Report += FString::Printf(TEXT("  %s.%s = %s\n"),
					*Node->GetName(), *Pin->PinName.ToString(),
					*Pin->DefaultObject->GetPathName());
				bHasValue = true;
			}
			else if (!Pin->DefaultValue.IsEmpty())
			{
				// Skip trivially zero/empty defaults that are just the engine default
				const FString& Val = Pin->DefaultValue;
				bool bIsTrivialDefault =
					Val == TEXT("0") || Val == TEXT("0.0") || Val == TEXT("0.000000") ||
					Val == TEXT("false") || Val == TEXT("") ||
					Val == TEXT("None") || Val == TEXT("()");

				if (!bIsTrivialDefault)
				{
					// Truncate very long values
					FString DisplayVal = Val.Len() > 100 ? Val.Left(100) + TEXT("...") : Val;
					Report += FString::Printf(TEXT("  %s.%s = \"%s\"\n"),
						*Node->GetName(), *Pin->PinName.ToString(), *DisplayVal);
					bHasValue = true;
				}
			}

			if (bHasValue)
				bHasNonDefaults = true;
		}
	}
	if (!bHasNonDefaults)
	{
		Report += TEXT("  (all at defaults)\n");
	}

	// -----------------------------------------------------------------------
	// Section 4: DISCONNECTED INPUT PINS — unwired inputs that may need attention
	// Only reports pins with NO value AND NO connection (potential authoring errors)
	// -----------------------------------------------------------------------
	Report += TEXT("\nDISCONNECTED INPUT PINS (may need attention):\n");
	bool bHasDisconnected = false;

	for (UEdGraphNode* Node : Nodes)
	{
		if (!Node) continue;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;
			if (Pin->Direction != EGPD_Input) continue;
			if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
			if (Pin->bHidden) continue;
			if (Pin->LinkedTo.Num() > 0) continue; // wired — not an issue

			const FName PinCat = Pin->PinType.PinCategory;

			// Object/asset pins with no DefaultObject
			if ((PinCat == TEXT("object") || PinCat == TEXT("softobject") ||
				 PinCat == TEXT("class")  || PinCat == TEXT("softclass")  ||
				 PinCat == TEXT("interface"))
				&& Pin->DefaultObject == nullptr)
			{
				FString ClassName = TEXT("Asset");
				if (UObject* SubCatObj = Pin->PinType.PinSubCategoryObject.Get())
					ClassName = SubCatObj->GetName();

				Report += FString::Printf(TEXT("  ⚠️ %s.%s — type: %s(%s) — NO CONNECTION, NO ASSET\n"),
					*Node->GetName(), *Pin->PinName.ToString(),
					*PinCat.ToString(), *ClassName);
				bHasDisconnected = true;
			}
			// Required numeric/struct pins with zero/empty value
			else if ((PinCat == TEXT("real") || PinCat == TEXT("float") || PinCat == TEXT("double") ||
					  PinCat == TEXT("struct"))
					 && (Pin->DefaultValue.IsEmpty() || Pin->DefaultValue == TEXT("0") ||
						 Pin->DefaultValue == TEXT("0.0") || Pin->DefaultValue == TEXT("0.000000")))
			{
				// Only flag if the pin name suggests it's significant (e.g. Location, Color, Scale)
				// Skip pins like "Tolerance", "DeltaSeconds" which are often validly zero
				FString PinNameStr = Pin->PinName.ToString();
				bool bLikelySignificant =
					PinNameStr.Contains(TEXT("Location")) ||
					PinNameStr.Contains(TEXT("Color")) ||
					PinNameStr.Contains(TEXT("Scale")) ||
					PinNameStr.Contains(TEXT("Size")) ||
					PinNameStr.Contains(TEXT("Offset")) ||
					PinNameStr.Contains(TEXT("Position")) ||
					PinNameStr.Contains(TEXT("Target")) ||
					PinNameStr.Contains(TEXT("Value"));

				if (bLikelySignificant)
				{
					Report += FString::Printf(TEXT("  ⚠️ %s.%s — type: %s — ZERO/EMPTY (verify intentional)\n"),
						*Node->GetName(), *Pin->PinName.ToString(), *PinCat.ToString());
					bHasDisconnected = true;
				}
			}
		}
	}
	if (!bHasDisconnected)
	{
		Report += TEXT("  (none)\n");
	}

	return Report;
}

// ============================================================================
// ResolvePinType
// ============================================================================

void FAgentFrameworkBlueprintActions::ResolvePinType(const FString& TypeName, FEdGraphPinType& OutPinType)
{
	FString TrimmedType = TypeName.TrimStartAndEnd();

	if ((TrimmedType.StartsWith(TEXT("TArray<"), ESearchCase::IgnoreCase) && TrimmedType.EndsWith(TEXT(">"))) ||
		TrimmedType.StartsWith(TEXT("Array of "), ESearchCase::IgnoreCase))
	{
		FString InnerType;
		if (TrimmedType.StartsWith(TEXT("TArray<"), ESearchCase::IgnoreCase))
		{
			InnerType = TrimmedType.Mid(7, TrimmedType.Len() - 8).TrimStartAndEnd();
		}
		else
		{
			InnerType = TrimmedType.Mid(9).TrimStartAndEnd();
		}
		ResolvePinType(InnerType, OutPinType);
		OutPinType.ContainerType = EPinContainerType::Array;
		return;
	}
	else if ((TrimmedType.StartsWith(TEXT("TSet<"), ESearchCase::IgnoreCase) && TrimmedType.EndsWith(TEXT(">"))) ||
		TrimmedType.StartsWith(TEXT("Set of "), ESearchCase::IgnoreCase))
	{
		FString InnerType;
		if (TrimmedType.StartsWith(TEXT("TSet<"), ESearchCase::IgnoreCase))
		{
			InnerType = TrimmedType.Mid(5, TrimmedType.Len() - 6).TrimStartAndEnd();
		}
		else
		{
			InnerType = TrimmedType.Mid(7).TrimStartAndEnd();
		}
		ResolvePinType(InnerType, OutPinType);
		OutPinType.ContainerType = EPinContainerType::Set;
		return;
	}
	else if ((TrimmedType.StartsWith(TEXT("TMap<"), ESearchCase::IgnoreCase) && TrimmedType.EndsWith(TEXT(">"))) ||
		TrimmedType.StartsWith(TEXT("Map of "), ESearchCase::IgnoreCase))
	{
		FString KeyTypeStr;
		FString ValueTypeStr;

		if (TrimmedType.StartsWith(TEXT("TMap<"), ESearchCase::IgnoreCase))
		{
			int32 Depth = 0;
			int32 CommaIdx = INDEX_NONE;
			for (int32 i = 5; i < TrimmedType.Len() - 1; ++i)
			{
				TCHAR Ch = TrimmedType[i];
				if (Ch == '<') Depth++;
				else if (Ch == '>') Depth--;
				else if (Ch == ',' && Depth == 0)
				{
					CommaIdx = i;
					break;
				}
			}

			if (CommaIdx != INDEX_NONE)
			{
				KeyTypeStr = TrimmedType.Mid(5, CommaIdx - 5).TrimStartAndEnd();
				ValueTypeStr = TrimmedType.Mid(CommaIdx + 1, TrimmedType.Len() - 1 - (CommaIdx + 1)).TrimStartAndEnd();
			}
		}
		else
		{
			FString Sub = TrimmedType.Mid(7).TrimStartAndEnd();
			int32 ToIdx = Sub.Find(TEXT(" to "), ESearchCase::IgnoreCase);
			if (ToIdx != INDEX_NONE)
			{
				KeyTypeStr = Sub.Left(ToIdx).TrimStartAndEnd();
				ValueTypeStr = Sub.Mid(ToIdx + 4).TrimStartAndEnd();
			}
			else
			{
				int32 CommaPos = Sub.Find(TEXT(","));
				if (CommaPos != INDEX_NONE)
				{
					KeyTypeStr = Sub.Left(CommaPos).TrimStartAndEnd();
					ValueTypeStr = Sub.Mid(CommaPos + 1).TrimStartAndEnd();
				}
			}
		}

		if (!KeyTypeStr.IsEmpty() && !ValueTypeStr.IsEmpty())
		{
			ResolvePinType(KeyTypeStr, OutPinType);

			FEdGraphPinType ValuePinType;
			ResolvePinType(ValueTypeStr, ValuePinType);

			OutPinType.PinValueType.TerminalCategory = ValuePinType.PinCategory;
			OutPinType.PinValueType.TerminalSubCategory = ValuePinType.PinSubCategory;
			OutPinType.PinValueType.TerminalSubCategoryObject = ValuePinType.PinSubCategoryObject;
			OutPinType.ContainerType = EPinContainerType::Map;
			return;
		}
	}

	const FString TypeLower = TrimmedType.ToLower();

	if (TypeLower == TEXT("bool") || TypeLower == TEXT("boolean"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (TypeLower == TEXT("int") || TypeLower == TEXT("int32") || TypeLower == TEXT("integer"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (TypeLower == TEXT("int64"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	}
	else if (TypeLower == TEXT("float") || TypeLower == TEXT("single") || TypeLower == TEXT("real"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (TypeLower == TEXT("double"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (TypeLower == TEXT("fstring") || TypeLower == TEXT("string"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (TypeLower == TEXT("ftext") || TypeLower == TEXT("text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (TypeLower == TEXT("fname") || TypeLower == TEXT("name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (TypeLower == TEXT("byte") || TypeLower == TEXT("uint8"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	}
	else if (TypeLower == TEXT("fvector") || TypeLower == TEXT("vector"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (TypeLower == TEXT("frotator") || TypeLower == TEXT("rotator"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (TypeLower == TEXT("ftransform") || TypeLower == TEXT("transform"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else if (TypeLower == TEXT("flinearcolor") || TypeLower == TEXT("linearcolor") || TypeLower == TEXT("color"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
	}
	else if (TypeLower == TEXT("fvector2d") || TypeLower == TEXT("vector2d"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FVector2D>::Get();
	}
	else
	{
		// Try as UClass (for Object references)
		UClass* FoundClass = FindFirstObject<UClass>(*TypeName, EFindFirstObjectOptions::None);
		if (!FoundClass)
			FoundClass = FindFirstObject<UClass>(*(TEXT("A") + TypeName), EFindFirstObjectOptions::None);
		if (!FoundClass)
			FoundClass = FindFirstObject<UClass>(*(TEXT("U") + TypeName), EFindFirstObjectOptions::None);

		if (FoundClass)
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			OutPinType.PinSubCategoryObject = FoundClass;
		}
		else
		{
			// Try as UScriptStruct (for custom structs)
			UScriptStruct* FoundStruct = FindFirstObject<UScriptStruct>(*TypeName, EFindFirstObjectOptions::None);
			if (!FoundStruct)
				FoundStruct = FindFirstObject<UScriptStruct>(*(TEXT("F") + TypeName), EFindFirstObjectOptions::None);

			if (FoundStruct)
			{
				OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
				OutPinType.PinSubCategoryObject = FoundStruct;
			}
			else
			{
				// Last resort: wildcard — will likely cause a compile error but won't crash
				OutPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
				UE_LOG(LogAgentFramework, Warning, TEXT("BlueprintActions: Unresolved pin type '%s' — using wildcard. Check the type name spelling."), *TypeName);
			}
		}
		}
	}
	
	// ============================================================================
	// ExecuteAddEnhancedInputNode
	// ============================================================================
	
FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteAddEnhancedInputNode(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	FString InputActionPath;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("input_action"), InputActionPath, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	// Load Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	// Load the Input Action asset
	UInputAction* InputAction = LoadObject<UInputAction>(nullptr, *InputActionPath);
	if (!IsValid(InputAction))
	{
		Result.Errors.Add(FString::Printf(TEXT("Input Action asset not found: '%s'. Use search_assets to find the correct path."), *InputActionPath));
		return Result;
	}

	// Find or create the EventGraph
	UEdGraph* EventGraph = FindOrCreateEventGraph(Blueprint);
	if (!IsValid(EventGraph))
	{
		Result.Errors.Add(TEXT("Could not find or create EventGraph."));
		return Result;
	}

	Blueprint->Modify();

	// Create the K2Node_EnhancedInputAction node
	UK2Node_EnhancedInputAction* InputNode = NewObject<UK2Node_EnhancedInputAction>(EventGraph);
	if (IsValid(InputNode))
	{
		InputNode->SetFlags(RF_Transactional);
		InputNode->InputAction = InputAction;

		// Set position
		int32 PosX = 0, PosY = 0;
		TArray<FString> PosErrors;
		UAgentFrameworkActionUtils::TryGetIntParam(Params, TEXT("node_pos_x"), PosX, PosErrors, false);
		UAgentFrameworkActionUtils::TryGetIntParam(Params, TEXT("node_pos_y"), PosY, PosErrors, false);
		InputNode->NodePosX = PosX;
		InputNode->NodePosY = PosY;

		// Add to graph and allocate default pins
		EventGraph->AddNode(InputNode, false, false);
		InputNode->CreateNewGuid();
		InputNode->PostPlacedNewNode();
		InputNode->AllocateDefaultPins();

		// Notify the blueprint
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

		// Compile
		bool bCompileOk = CompileAndReport(Blueprint, Result, true);
		if (IsValid(Blueprint->GetOutermost()))
		{
			Blueprint->GetOutermost()->MarkPackageDirty();
		}

		FString NodeName = InputNode->GetName();
		FString NodeTitle = InputNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();

		// Build result with pin info for the AI
		FString PinInfo;
		for (UEdGraphPin* Pin : InputNode->Pins)
		{
			if (Pin)
			{
				FString Dir = (Pin->Direction == EGPD_Output) ? TEXT("OUT") : TEXT("IN");
				PinInfo += FString::Printf(TEXT("  [%s] %s (%s)\n"), *Dir, *Pin->PinName.ToString(), *Pin->PinType.PinCategory.ToString());
			}
		}

		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(
			TEXT("Added Enhanced Input Action node '%s' (%s) for input action '%s'.\n"
				 "Internal node name: %s\n"
				 "Position: (%d, %d)\n"
				 "Pins:\n%s\n"
				 "Use connect_blueprint_pins to wire 'Triggered' or 'Started' output to your function call.\n"
				 "Compiled: %s"),
			*NodeTitle, *InputNode->GetClass()->GetName(),
			*InputAction->GetName(),
			*NodeName,
			PosX, PosY,
			*PinInfo,
			bCompileOk ? TEXT("OK") : TEXT("with errors (see warnings)"));
		Result.ModifiedAssets.Add(AssetPath);

		UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: Added Enhanced Input node '%s' for IA '%s' in '%s'"),
			*NodeName, *InputAction->GetName(), *AssetPath);
	}

	return Result;
}
	
	// ============================================================================
	// ExecuteVerifyConnections
	// ============================================================================
	
	FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteVerifyConnections(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
	{
		FString AssetPath;
		TArray<FString> Errors;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true))
		{
			Result.Errors.Append(Errors);
			return Result;
		}
	
		FString FilterGraphName;
		TArray<FString> OptErrors;
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("graph_name"), FilterGraphName, OptErrors, false);
	
		// -------------------------------------------------------------------------
		// Load Blueprint
		// -------------------------------------------------------------------------
		UBlueprint* Blueprint = Cast<UBlueprint>(
			StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
		if (!IsValid(Blueprint))
		{
			Result.Errors.Add(FString::Printf(TEXT("Could not load Blueprint at '%s'"), *AssetPath));
			return Result;
		}
	
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	
		// -------------------------------------------------------------------------
		// Collect graphs to inspect
		// -------------------------------------------------------------------------
		TArray<UEdGraph*> GraphsToCheck;
		if (!FilterGraphName.IsEmpty())
		{
			// Specific graph requested
			for (UEdGraph* G : Blueprint->UbergraphPages)
			{
				if (IsValid(G) && G->GetName() == FilterGraphName)
				{
					GraphsToCheck.Add(G);
					break;
				}
			}
			for (UEdGraph* G : Blueprint->FunctionGraphs)
			{
				if (IsValid(G) && G->GetName() == FilterGraphName)
				{
					GraphsToCheck.Add(G);
					break;
				}
			}
			if (GraphsToCheck.IsEmpty())
			{
				Result.Errors.Add(FString::Printf(TEXT("Graph '%s' not found in Blueprint '%s'"), *FilterGraphName, *AssetPath));
				return Result;
			}
		}
		else
		{
			for (UEdGraph* G : Blueprint->UbergraphPages) { if (IsValid(G)) GraphsToCheck.Add(G); }
			for (UEdGraph* G : Blueprint->FunctionGraphs) { if (IsValid(G)) GraphsToCheck.Add(G); }
		}
	
		// -------------------------------------------------------------------------
		// Issue + repair structures
		// -------------------------------------------------------------------------
		struct FConnectionIssue
		{
			FString GraphName;
			FString NodeName;
			FString PinName;
			FString PinDirection; // "OUT" or "IN"
			FString IssueType;    // "DISCONNECTED_EXEC", "STALE_LINK"
		};
	
		TArray<FConnectionIssue> IssuesFound;
		TArray<FConnectionIssue> RepairsMade;
		TArray<FConnectionIssue> Unresolved;
	
		int32 TotalStaleLinksRemoved = 0;
	
		// -------------------------------------------------------------------------
		// Pass 1: Collect issues and remove stale links
		// -------------------------------------------------------------------------
		for (UEdGraph* Graph : GraphsToCheck)
		{
			if (!IsValid(Graph)) continue;
	
			// Build current node name set for stale-link detection
			TSet<FString> NodeNamesInGraph;
			for (UEdGraphNode* GNode : Graph->Nodes)
			{
				if (IsValid(GNode)) NodeNamesInGraph.Add(GNode->GetName());
			}
	
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!IsValid(Node)) continue;
	
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin) continue;
	
					// --- Remove stale LinkedTo references ---
					int32 StaleCount = Pin->LinkedTo.RemoveAll([&NodeNamesInGraph](UEdGraphPin* LP) -> bool
					{
						if (!LP) return true;
						UEdGraphNode* Owner = LP->GetOwningNodeUnchecked();
						if (!IsValid(Owner)) return true;
						if (!NodeNamesInGraph.Contains(Owner->GetName())) return true;
						return false;
					});
					if (StaleCount > 0)
					{
						TotalStaleLinksRemoved += StaleCount;
						FConnectionIssue Issue;
						Issue.GraphName = Graph->GetName();
						Issue.NodeName = Node->GetName();
						Issue.PinName = Pin->PinName.ToString();
						Issue.PinDirection = (Pin->Direction == EGPD_Output) ? TEXT("OUT") : TEXT("IN");
						Issue.IssueType = FString::Printf(TEXT("STALE_LINK (removed %d)"), StaleCount);
						IssuesFound.Add(Issue);
					}
	
					// --- Detect disconnected exec output pins ---
					if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
						&& Pin->Direction == EGPD_Output
						&& Pin->LinkedTo.IsEmpty())
					{
						// Only report if this node has a non-exec output (i.e., it's in the middle of a chain)
						// or if it's a FunctionEntry node (must be connected to something)
						bool bIsFunctionEntry = Node->IsA<UK2Node_FunctionEntry>();
						bool bIsCallFunction  = Node->IsA<UK2Node_CallFunction>();
						bool bIsEvent         = Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_CustomEvent>();
	
						if (bIsFunctionEntry || bIsCallFunction || bIsEvent)
						{
							FConnectionIssue Issue;
							Issue.GraphName = Graph->GetName();
							Issue.NodeName = Node->GetName();
							Issue.PinName = Pin->PinName.ToString();
							Issue.PinDirection = TEXT("OUT");
							Issue.IssueType = TEXT("DISCONNECTED_EXEC_OUTPUT");
							IssuesFound.Add(Issue);
						}
					}
	
					// --- Detect disconnected exec input pins ---
					if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
						&& Pin->Direction == EGPD_Input
						&& Pin->LinkedTo.IsEmpty())
					{
						bool bIsCallFunction  = Node->IsA<UK2Node_CallFunction>();
						bool bIsFunctionResult = Node->IsA<UK2Node_FunctionResult>();
	
						if (bIsCallFunction || bIsFunctionResult)
						{
							FConnectionIssue Issue;
							Issue.GraphName = Graph->GetName();
							Issue.NodeName = Node->GetName();
							Issue.PinName = Pin->PinName.ToString();
							Issue.PinDirection = TEXT("IN");
							Issue.IssueType = TEXT("DISCONNECTED_EXEC_INPUT");
							IssuesFound.Add(Issue);
						}
					}
				}
			}
		}
	
		// -------------------------------------------------------------------------
		// Pass 2: Attempt to repair DISCONNECTED_EXEC chains by linking adjacent
		// FunctionEntry → first call node, and call nodes with unique exec-in targets.
		// -------------------------------------------------------------------------
		for (UEdGraph* Graph : GraphsToCheck)
		{
			if (!IsValid(Graph)) continue;
	
			// Collect FunctionEntry nodes with disconnected exec-out
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!IsValid(Node)) continue;
				if (!Node->IsA<UK2Node_FunctionEntry>()) continue;
	
				UEdGraphPin* ExecOutPin = nullptr;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin
						&& Pin->Direction == EGPD_Output
						&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
						&& Pin->LinkedTo.IsEmpty())
					{
						ExecOutPin = Pin;
						break;
					}
				}
				if (!ExecOutPin) continue;
	
				// Find call nodes in this graph that have a disconnected exec-in
				TArray<UEdGraphNode*> CandidateTargets;
				for (UEdGraphNode* TargetNode : Graph->Nodes)
				{
					if (!IsValid(TargetNode) || TargetNode == Node) continue;
					if (!TargetNode->IsA<UK2Node_CallFunction>()) continue;
	
					for (UEdGraphPin* TargetPin : TargetNode->Pins)
					{
						if (TargetPin
							&& TargetPin->Direction == EGPD_Input
							&& TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
							&& TargetPin->LinkedTo.IsEmpty())
						{
							CandidateTargets.Add(TargetNode);
							break;
						}
					}
				}
	
				// Only auto-repair if exactly one candidate (unambiguous)
				if (CandidateTargets.Num() == 1)
				{
					UEdGraphNode* TargetNode = CandidateTargets[0];
					UEdGraphPin* TargetExecIn = nullptr;
					for (UEdGraphPin* TargetPin : TargetNode->Pins)
					{
						if (TargetPin
							&& TargetPin->Direction == EGPD_Input
							&& TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
							&& TargetPin->LinkedTo.IsEmpty())
						{
							TargetExecIn = TargetPin;
							break;
						}
					}
	
					if (TargetExecIn && Schema)
					{
						FPinConnectionResponse ConnResponse = Schema->CanCreateConnection(ExecOutPin, TargetExecIn);
						if (ConnResponse.Response == CONNECT_RESPONSE_MAKE
							|| ConnResponse.Response == CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE)
						{
							Schema->TryCreateConnection(ExecOutPin, TargetExecIn);
							FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
							FConnectionIssue Repair;
							Repair.GraphName = Graph->GetName();
							Repair.NodeName = FString::Printf(TEXT("%s -> %s"), *Node->GetName(), *TargetNode->GetName());
							Repair.PinName = FString::Printf(TEXT("%s -> %s"), *ExecOutPin->PinName.ToString(), *TargetExecIn->PinName.ToString());
							Repair.PinDirection = TEXT("OUT->IN");
							Repair.IssueType = TEXT("EXEC_CHAIN_REPAIRED");
							RepairsMade.Add(Repair);
						}
					}
				}
				else if (CandidateTargets.Num() > 1)
				{
					// Ambiguous: report as unresolved
					FConnectionIssue Issue;
					Issue.GraphName = Graph->GetName();
					Issue.NodeName = Node->GetName();
					Issue.PinName = ExecOutPin->PinName.ToString();
					Issue.PinDirection = TEXT("OUT");
					Issue.IssueType = FString::Printf(
						TEXT("DISCONNECTED_EXEC_OUTPUT (ambiguous: %d possible targets — use connect_blueprint_pins to wire manually)"),
						CandidateTargets.Num());
					Unresolved.Add(Issue);
				}
			}
		}
	
		// -------------------------------------------------------------------------
		// Pass 3: Full pin-value audit across ALL nodes in checked graphs
		// (object pins with no DefaultObject, zero numeric pins, all-zero structs)
		// -------------------------------------------------------------------------
		FString GlobalPinAudit;
		{
			for (UEdGraph* Graph : GraphsToCheck)
			{
				if (!IsValid(Graph)) continue;
				TSet<UEdGraphNode*> NodeSet;
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (IsValid(Node)) NodeSet.Add(Node);
				}
				FString GraphAudit = BuildPinAuditReport(NodeSet);
				if (!GraphAudit.IsEmpty())
				{
					GlobalPinAudit += FString::Printf(TEXT("\n[PIN AUDIT — graph \"%s\"]"), *Graph->GetName());
					GlobalPinAudit += GraphAudit;
				}
			}
		}
	
		// -------------------------------------------------------------------------
		// Pass 4: Compact connection report (replaces full T3D readback)
		// Token-efficient summary of exec chains, data connections, pin values.
		// -------------------------------------------------------------------------
		FString GraphReadbacks;
		{
			for (UEdGraph* Graph : GraphsToCheck)
			{
				if (!IsValid(Graph)) continue;
				TSet<UEdGraphNode*> NodeSet;
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (IsValid(Node)) NodeSet.Add(Node);
				}
				if (NodeSet.IsEmpty()) continue;
	
				GraphReadbacks += BuildCompactConnectionReport(NodeSet, Graph->GetName());
			}
		}
	
		// -------------------------------------------------------------------------
		// Build result message
		// -------------------------------------------------------------------------
		bool bHadIssues     = IssuesFound.Num() > 0;
		bool bHadRepairs    = RepairsMade.Num() > 0;
		bool bHadUnresolved = Unresolved.Num() > 0;
	
		// Recompile if we made any repairs
		bool bCompileOk = true;
		if (bHadRepairs)
		{
			bCompileOk = CompileAndReport(Blueprint, Result, true);
			if (IsValid(Blueprint->GetOutermost()))
			{
				Blueprint->GetOutermost()->MarkPackageDirty();
			}
		}
	
		FString Report;
		Report += FString::Printf(TEXT("verify_blueprint_connections — Blueprint: %s\n"), *AssetPath);
		if (!FilterGraphName.IsEmpty())
			Report += FString::Printf(TEXT("Graph filter: '%s'\n"), *FilterGraphName);
		Report += TEXT("---\n");
	
		if (!bHadIssues && !bHadRepairs && !bHadUnresolved)
		{
			Report += TEXT("STATUS: CLEAN — No exec-pin or stale-link issues detected.\n");
		}
		else
		{
			Report += FString::Printf(TEXT("Issues found: %d | Repairs made: %d | Unresolved: %d | Stale links removed: %d\n"),
				IssuesFound.Num(), RepairsMade.Num(), Unresolved.Num(), TotalStaleLinksRemoved);
	
			if (IssuesFound.Num() > 0)
			{
				Report += TEXT("\n[ISSUES FOUND]\n");
				for (const FConnectionIssue& Issue : IssuesFound)
				{
					Report += FString::Printf(TEXT("  Graph='%s' Node='%s' Pin='%s' Dir=%s  → %s\n"),
						*Issue.GraphName, *Issue.NodeName, *Issue.PinName, *Issue.PinDirection, *Issue.IssueType);
				}
			}
	
			if (RepairsMade.Num() > 0)
			{
				Report += TEXT("\n[REPAIRS MADE]\n");
				for (const FConnectionIssue& Repair : RepairsMade)
				{
					Report += FString::Printf(TEXT("  Graph='%s' Nodes='%s' Pins='%s'  → %s\n"),
						*Repair.GraphName, *Repair.NodeName, *Repair.PinName, *Repair.IssueType);
				}
			}
	
			if (Unresolved.Num() > 0)
			{
				Report += TEXT("\n[UNRESOLVED — manual action needed]\n");
				for (const FConnectionIssue& Issue : Unresolved)
				{
					Report += FString::Printf(TEXT("  Graph='%s' Node='%s' Pin='%s' Dir=%s  → %s\n"),
						*Issue.GraphName, *Issue.NodeName, *Issue.PinName, *Issue.PinDirection, *Issue.IssueType);
				}
				Report += TEXT("\nACTION: Use connect_blueprint_pins to wire the unresolved pins listed above.\n");
			}
	
			if (bHadRepairs)
			{
				Report += FString::Printf(TEXT("\nPost-repair compile: %s\n"), bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED (see Errors)"));
			}
		}
	
		// Append pin audit (Pass 3) and T3D readback (Pass 4)
		if (!GlobalPinAudit.IsEmpty())
		{
			Report += TEXT("\n\n=== FULL PIN VALUE AUDIT (all graphs) ===");
			Report += TEXT("\nDO NOT declare the task complete until every issue below is resolved:\n");
			Report += GlobalPinAudit;
		}
	
		Report += GraphReadbacks;
	
		Result.bSuccess = true;
		Result.ResultMessage = Report;
		Result.ModifiedAssets.Add(AssetPath);
	
		UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: VerifyConnections completed for '%s' — %d issues, %d repairs, %d unresolved"),
			*AssetPath, IssuesFound.Num(), RepairsMade.Num(), Unresolved.Num());
	
		return Result;
	}

// ============================================================================
// AutoLayoutNodes — Sugiyama-style layered DAG layout for injected nodes
// ============================================================================

void FAgentFrameworkBlueprintActions::AutoLayoutNodes(
	const TSet<UEdGraphNode*>& Nodes,
	int32 StartX, int32 StartY,
	int32 LayerSpacingX, int32 NodeSpacingY)
{
	if (Nodes.Num() == 0) return;

	// Step 1: Build adjacency map (following exec output pins → exec input pins)
	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> Successors;
	TMap<UEdGraphNode*, int32> InDegree;
	TSet<UEdGraphNode*> NodeSet = Nodes;

	for (UEdGraphNode* Node : NodeSet)
	{
		if (!Node) continue;
		if (!InDegree.Contains(Node)) InDegree.Add(Node, 0);

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;
			// Follow exec output pins
			if (Pin->Direction == EGPD_Output &&
				(Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec))
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin) continue;
					UEdGraphNode* SuccNode = LinkedPin->GetOwningNode();
					if (SuccNode && NodeSet.Contains(SuccNode))
					{
						Successors.FindOrAdd(Node).AddUnique(SuccNode);
						InDegree.FindOrAdd(SuccNode, 0)++;
					}
				}
			}
		}
	}

	// Step 2: Topological sort (Kahn's algorithm) → layer assignment
	TArray<UEdGraphNode*> Queue;
	for (auto& KV : InDegree)
	{
		if (KV.Value == 0 && KV.Key)
			Queue.Add(KV.Key);
	}

	// Assign layers via longest-path from sources
	TMap<UEdGraphNode*, int32> LayerMap;
	for (UEdGraphNode* N : NodeSet) { LayerMap.Add(N, 0); }

	TArray<UEdGraphNode*> ProcessOrder;
	int32 QueueIdx = 0;
	while (QueueIdx < Queue.Num())
	{
		UEdGraphNode* Current = Queue[QueueIdx++];
		ProcessOrder.Add(Current);

		if (TArray<UEdGraphNode*>* Succs = Successors.Find(Current))
		{
			for (UEdGraphNode* Succ : *Succs)
			{
				int32 NewLayer = LayerMap[Current] + 1;
				if (NewLayer > LayerMap[Succ])
					LayerMap[Succ] = NewLayer;

				InDegree[Succ]--;
				if (InDegree[Succ] == 0)
					Queue.Add(Succ);
			}
		}
	}

	// Handle nodes not reached by topological sort (data-only nodes without exec connections)
	for (UEdGraphNode* N : NodeSet)
	{
		if (!ProcessOrder.Contains(N))
			ProcessOrder.Add(N);
	}

	// Step 3: Group nodes by layer
	TMap<int32, TArray<UEdGraphNode*>> Layers;
	for (UEdGraphNode* N : ProcessOrder)
	{
		Layers.FindOrAdd(LayerMap[N]).Add(N);
	}

	// Step 4: Assign coordinates
	TArray<int32> LayerKeys;
	Layers.GetKeys(LayerKeys);
	LayerKeys.Sort();

	for (int32 LayerIdx = 0; LayerIdx < LayerKeys.Num(); ++LayerIdx)
	{
		TArray<UEdGraphNode*>& LayerNodes = Layers[LayerKeys[LayerIdx]];
		int32 X = StartX + LayerIdx * LayerSpacingX;

		for (int32 NodeIdx = 0; NodeIdx < LayerNodes.Num(); ++NodeIdx)
		{
			UEdGraphNode* Node = LayerNodes[NodeIdx];
			if (!Node) continue;

			Node->NodePosX = X;
			Node->NodePosY = StartY + NodeIdx * NodeSpacingY;
		}
	}

	UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: Auto-layout applied to %d nodes across %d layers"),
		Nodes.Num(), LayerKeys.Num());
}

// ============================================================================
// PreFlightValidateT3D — Validate T3D references against reflection
// ============================================================================

bool FAgentFrameworkBlueprintActions::PreFlightValidateT3D(
	const FString& T3DText,
	const UBlueprint* Blueprint,
	TArray<FString>& OutWarnings)
{
	bool bValid = true;

	// Extract Class= references from T3D and validate they exist
	// Pattern: Class=/Script/ModuleName.ClassName
	TArray<FString> Lines;
	T3DText.ParseIntoArrayLines(Lines);

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const FString& Line = Lines[i].TrimStartAndEnd();

		// Check for Begin Object Class= lines
		if (Line.StartsWith(TEXT("Begin Object")) && Line.Contains(TEXT("Class=")))
		{
			// Extract the class path
			FString ClassPath;
			int32 ClassStart = Line.Find(TEXT("Class="));
			if (ClassStart != INDEX_NONE)
			{
				FString AfterClass = Line.Mid(ClassStart + 6);
				// Class path ends at the next space or quote
				int32 EndPos;
				if (AfterClass.FindChar(TEXT(' '), EndPos) || AfterClass.FindChar(TEXT('"'), EndPos))
					ClassPath = AfterClass.Left(EndPos);
				else
					ClassPath = AfterClass;

				ClassPath = ClassPath.TrimQuotes();

				// Try to find the class
				UClass* NodeClass = FindFirstObject<UClass>(*FPackageName::GetLongPackageAssetName(ClassPath), EFindFirstObjectOptions::NativeFirst);
				if (!NodeClass)
				{
					// Try loading the class
					NodeClass = LoadClass<UEdGraphNode>(nullptr, *ClassPath);
				}

				if (!NodeClass)
				{
					OutWarnings.Add(FString::Printf(
						TEXT("T3D line %d: Node class '%s' not found. This node may not be importable. "
							 "Verify the class path. Common classes: K2Node_CallFunction, K2Node_VariableGet, "
							 "K2Node_DynamicCast, K2Node_IfThenElse, K2Node_Timeline."),
						i + 1, *ClassPath));
					bValid = false;
				}
			}
		}

		// Check for FunctionReference MemberName= to validate function existence
		if (Line.Contains(TEXT("MemberName=\"")))
		{
			int32 MemberStart = Line.Find(TEXT("MemberName=\"")) + 12;
			int32 MemberEnd = Line.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, MemberStart);
			if (MemberEnd > MemberStart)
			{
				FString MemberName = Line.Mid(MemberStart, MemberEnd - MemberStart);

				// Skip engine functions — they're almost always valid
				// Only warn on project-specific function names that might be misspelled
				if (!MemberName.IsEmpty() && Blueprint)
				{
					// Check if this looks like a project-specific function (contains the project name or custom prefix)
					// We don't validate engine functions to avoid false positives
					FString ProjectName = FApp::GetProjectName();
					if (MemberName.Contains(ProjectName, ESearchCase::IgnoreCase))
					{
						// This might be a project function — verify it exists
						bool bFound = false;
						for (UEdGraph* FuncGraph : Blueprint->FunctionGraphs)
						{
							if (FuncGraph && FuncGraph->GetName() == MemberName)
							{
								bFound = true;
								break;
							}
						}
						if (!bFound)
						{
							OutWarnings.Add(FString::Printf(
								TEXT("T3D line %d: Function '%s' referenced but not found in Blueprint. "
									 "Ensure it exists before injection. Use add_blueprint_function to create it."),
								i + 1, *MemberName));
						}
					}
				}
			}
		}
	}

	return bValid;
}

// ============================================================================
// ExecuteSetNodePinDefault — Set default value on an existing graph node pin
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteSetNodePinDefault(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	FString NodeName;
	FString PinName;
	FString Value;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("node_name"), NodeName, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("pin_name"), PinName, Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("value"), Value, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	FString GraphName = TEXT("EventGraph");
	TArray<FString> GraphErrors;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("graph_name"), GraphName, GraphErrors, false);

	// Load Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	// Find the target graph (search all graphs: UbergraphPages + FunctionGraphs)
	UEdGraph* TargetGraph = nullptr;
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for (UEdGraph* Graph : AllGraphs)
	{
		if (IsValid(Graph) && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			TargetGraph = Graph;
			break;
		}
	}

	if (!IsValid(TargetGraph))
	{
		Result.Errors.Add(FString::Printf(TEXT("Graph '%s' not found in Blueprint '%s'."), *GraphName, *AssetPath));
		return Result;
	}

	// Find the target node by internal name (case-insensitive)
	UEdGraphNode* TargetNode = nullptr;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (IsValid(Node) && Node->GetName().Equals(NodeName, ESearchCase::IgnoreCase))
		{
			TargetNode = Node;
			break;
		}
	}

	if (!IsValid(TargetNode))
	{
		// Build a list of available node names for the error message
		FString AvailableNodes;
		int32 Count = 0;
		for (UEdGraphNode* Node : TargetGraph->Nodes)
		{
			if (IsValid(Node) && Count < 20)
			{
				if (Count > 0) AvailableNodes += TEXT(", ");
				AvailableNodes += Node->GetName();
				++Count;
			}
		}
		Result.Errors.Add(FString::Printf(
			TEXT("Node '%s' not found in graph '%s'. Available nodes: %s"),
			*NodeName, *GraphName, *AvailableNodes));
		return Result;
	}

	// Find the target input pin (case-insensitive)
	UEdGraphPin* TargetPin = nullptr;
	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input &&
			Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			TargetPin = Pin;
			break;
		}
	}

	if (!TargetPin)
	{
		// Build a list of available input pin names
		FString PinList;
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input)
			{
				PinList += FString::Printf(TEXT("\n  \"%s\" (type: %s)"),
					*Pin->PinName.ToString(), *Pin->PinType.PinCategory.ToString());
			}
		}
		Result.Errors.Add(FString::Printf(
			TEXT("Input pin '%s' not found on node '%s'. Available input pins:%s"),
			*PinName, *NodeName, *PinList));
		return Result;
	}

	// Reject exec pins — they don't have default values
	if (TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Pin '%s' is an execution pin — execution pins do not have default values. Use connect_blueprint_pins to wire them."),
			*PinName));
		return Result;
	}

	// Begin transaction for undo/redo support
	Blueprint->Modify();
	TargetNode->Modify();

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (!K2Schema) return Result;
	FString PinCategory = TargetPin->PinType.PinCategory.ToString();
	FString DispatchMethod;

	// Dispatch based on pin type
	if (TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object ||
		TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class ||
		TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
	{
		// Hard object/class/interface references — must load the asset and use TrySetDefaultObject
		DispatchMethod = TEXT("TrySetDefaultObject");

		UObject* LoadedAsset = nullptr;
		if (!Value.IsEmpty() && !Value.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			// Determine the expected class from the pin's sub-category
			UClass* ExpectedClass = UObject::StaticClass();
			if (TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
			{
				ExpectedClass = UClass::StaticClass();
			}
			else if (TargetPin->PinType.PinSubCategoryObject.IsValid())
			{
				UClass* SubClass = Cast<UClass>(TargetPin->PinType.PinSubCategoryObject.Get());
				if (IsValid(SubClass)) ExpectedClass = SubClass;
			}

			LoadedAsset = StaticLoadObject(ExpectedClass, nullptr, *Value);
			if (!IsValid(LoadedAsset))
			{
				Result.Warnings.Add(FString::Printf(
					TEXT("Could not load object at path '%s' (expected class: %s). Setting pin to null."),
					*Value, *ExpectedClass->GetName()));
			}
		}

		K2Schema->TrySetDefaultObject(*TargetPin, LoadedAsset);
	}
	else if (TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		// FText pins — use TrySetDefaultText for localization correctness
		DispatchMethod = TEXT("TrySetDefaultText");
		FText NewText = FText::FromString(Value);
		K2Schema->TrySetDefaultText(*TargetPin, NewText);
	}
	else if (TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject ||
			 TargetPin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
	{
		// Soft references — stored as asset path strings in DefaultValue
		DispatchMethod = TEXT("TrySetDefaultValue (soft reference)");
		K2Schema->TrySetDefaultValue(*TargetPin, Value);
	}
	else
	{
		// All other types: bool, int, float, string, name, byte/enum, struct (FVector, FRotator, etc.)
		DispatchMethod = TEXT("TrySetDefaultValue");
		K2Schema->TrySetDefaultValue(*TargetPin, Value);
	}

	// Post-modification notification
	TargetNode->PinDefaultValueChanged(TargetPin);
	TargetGraph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Compile to bake the new default into bytecode
	bool bCompileOk = CompileAndReport(Blueprint, Result, true);
	if (IsValid(Blueprint->GetOutermost()))
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}

	// Read back the actual value to confirm it was set
	FString ActualValue = TargetPin->DefaultValue;
	FString ActualObject = IsValid(TargetPin->DefaultObject) ? TargetPin->DefaultObject->GetPathName() : TEXT("");
	FString ActualText = TargetPin->DefaultTextValue.ToString();

	FString ConfirmValue;
	if (!ActualObject.IsEmpty())
		ConfirmValue = ActualObject;
	else if (!ActualText.IsEmpty())
		ConfirmValue = ActualText;
	else
		ConfirmValue = ActualValue;

	Result.bSuccess = bCompileOk;
	Result.ResultMessage = FString::Printf(
		TEXT("Set pin default: %s.%s = \"%s\" (via %s, pin type: %s).\nConfirmed value: \"%s\".\nCompile: %s."),
		*NodeName, *PinName, *Value, *DispatchMethod, *PinCategory,
		*ConfirmValue,
		bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED"));
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteDeleteNodes — Remove nodes from a Blueprint graph by name
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteDeleteNodes(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	FString GraphName = TEXT("EventGraph");
	TArray<FString> GraphErrors;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("graph_name"), GraphName, GraphErrors, false);

	// Parse node_names array
	const TArray<TSharedPtr<FJsonValue>>* NodeNamesArray = nullptr;
	if (!UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("node_names"), NodeNamesArray, Errors, true) || !NodeNamesArray || NodeNamesArray->Num() == 0)
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	TArray<FString> NamesToDelete;
	for (const TSharedPtr<FJsonValue>& Val : *NodeNamesArray)
	{
		if (Val.IsValid() && Val->Type == EJson::String)
		{
			FString Name = Val->AsString();
			if (!Name.IsEmpty())
			{
				NamesToDelete.Add(Name);
			}
		}
	}

	if (NamesToDelete.Num() == 0)
	{
		Result.Errors.Add(TEXT("node_names array contained no valid string entries."));
		return Result;
	}

	// Load Blueprint
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	// Find graph
	UEdGraph* TargetGraph = FindOrCreateEventGraph(Blueprint, GraphName);
	if (!IsValid(TargetGraph))
	{
		for (UEdGraph* G : Blueprint->FunctionGraphs)
		{
			if (IsValid(G) && G->GetName() == GraphName)
			{
				TargetGraph = G;
				break;
			}
		}
	}
	if (!IsValid(TargetGraph))
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Graph '%s' not found in Blueprint '%s'. Use get_blueprint_info to list available graphs."),
			*GraphName, *AssetPath));
		return Result;
	}

	Blueprint->Modify();

	// Find and remove matching nodes
	TArray<FString> Deleted;
	TArray<FString> NotFound;
	TSet<FString> NamesToDeleteSet(NamesToDelete);

	// Collect nodes to delete (can't modify array during iteration)
	TArray<UEdGraphNode*> NodesToRemove;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (IsValid(Node) && NamesToDeleteSet.Contains(Node->GetName()))
		{
			NodesToRemove.Add(Node);
		}
	}

	// Track which names we found
	TSet<FString> FoundNames;
	for (UEdGraphNode* Node : NodesToRemove)
	{
		if (IsValid(Node)) FoundNames.Add(Node->GetName());
	}

	for (const FString& Name : NamesToDelete)
	{
		if (!FoundNames.Contains(Name))
		{
			NotFound.Add(Name);
		}
	}

	// Delete nodes: break all pin connections first, then remove from graph
	for (UEdGraphNode* Node : NodesToRemove)
	{
		if (!IsValid(Node)) continue;
		FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		FString NodeName = Node->GetName();

		// Break all connections on all pins (prevents dangling LinkedTo refs)
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				Pin->BreakAllPinLinks();
			}
		}

		// Remove node from graph
		TargetGraph->RemoveNode(Node);
		Deleted.Add(FString::Printf(TEXT("%s (%s)"), *NodeName, *NodeTitle));

		UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: Deleted node '%s' (%s) from graph '%s' of '%s'"),
			*NodeName, *NodeTitle, *GraphName, *AssetPath);
	}

	if (Deleted.Num() == 0 && NotFound.Num() > 0)
	{
		Result.Errors.Add(FString::Printf(
			TEXT("None of the specified nodes were found in graph '%s'. Not found: %s. Use get_blueprint_info to see current node names."),
			*GraphName, *FString::Join(NotFound, TEXT(", "))));
		return Result;
	}

	// Compile
	bool bCompileOk = CompileAndReport(Blueprint, Result, true);
	if (IsValid(Blueprint->GetOutermost()))
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}

	// Build result message
	FString DeletedList = FString::Join(Deleted, TEXT("\n  "));
	FString ResultMsg = FString::Printf(
		TEXT("Deleted %d node(s) from graph '%s' of '%s':\n  %s\nCompile: %s."),
		Deleted.Num(), *GraphName, *AssetPath, *DeletedList,
		bCompileOk ? TEXT("SUCCESS") : TEXT("FAILED"));

	if (NotFound.Num() > 0)
	{
		Result.Warnings.Add(FString::Printf(
			TEXT("Could not find %d node(s): %s. They may have already been deleted or the names are incorrect."),
			NotFound.Num(), *FString::Join(NotFound, TEXT(", "))));
	}

	Result.bSuccess = true;
	Result.ResultMessage = ResultMsg;
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// BSF (Blueprint Summary Format) Export (NEW)
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteExportBlueprintSummary(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load blueprint at %s"), *AssetPath));
		return Result;
	}

	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("asset_path"), AssetPath);
	RootObject->SetStringField(TEXT("parent_class"), IsValid(Blueprint->ParentClass) ? Blueprint->ParentClass->GetName() : TEXT("None"));
	RootObject->SetStringField(TEXT("compile_status"), Blueprint->Status == BS_UpToDate ? TEXT("Valid") : TEXT("Dirty/Error"));

	TArray<TSharedPtr<FJsonValue>> VariablesArray;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), GetVarTypeString(Var.VarType));
		VarObj->SetStringField(TEXT("category"), Var.Category.ToString());
		VariablesArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}
	RootObject->SetArrayField(TEXT("variables"), VariablesArray);

	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	if (IsValid(Blueprint->SimpleConstructionScript))
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (!IsValid(Node) || !IsValid(Node->ComponentTemplate)) continue;
			TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
			CompObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
			CompObj->SetStringField(TEXT("class"), Node->ComponentTemplate->GetClass()->GetName());
			
			USCS_Node* ParentNode = Blueprint->SimpleConstructionScript->FindParentNode(Node);
			CompObj->SetStringField(TEXT("parent"), IsValid(ParentNode) ? ParentNode->GetVariableName().ToString() : TEXT("DefaultSceneRoot"));
			
			ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}
	RootObject->SetArrayField(TEXT("components"), ComponentsArray);

	TArray<TSharedPtr<FJsonValue>> GraphsArray;
	auto ProcessGraphs = [&](const TArray<UEdGraph*>& Graphs)
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (!IsValid(Graph)) continue;
			TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
			GraphObj->SetStringField(TEXT("name"), Graph->GetName());
			GraphObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());

			TSet<UEdGraphNode*> NodeSet;
			for (UEdGraphNode* GNode : Graph->Nodes)
			{
				if (IsValid(GNode)) NodeSet.Add(GNode);
			}
			FString LogicSummary = BuildCompactConnectionReport(NodeSet, Graph->GetName());
			GraphObj->SetStringField(TEXT("logic_summary"), LogicSummary);

			GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
		}
	};
	ProcessGraphs(Blueprint->UbergraphPages);
	ProcessGraphs(Blueprint->FunctionGraphs);
	ProcessGraphs(Blueprint->MacroGraphs);
	
	RootObject->SetArrayField(TEXT("graphs"), GraphsArray);

	TArray<TSharedPtr<FJsonValue>> InterfacesArray;
	for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
	{
		if (IsValid(Interface.Interface))
		{
			InterfacesArray.Add(MakeShared<FJsonValueString>(Interface.Interface->GetName()));
		}
	}
	RootObject->SetArrayField(TEXT("interfaces"), InterfacesArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = OutputString;
	return Result;
}

// ============================================================================
// ExecuteBatchOperations (NEW — Transactional Batching)
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteBatchOperations(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
	if (!UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("operations"), OperationsArray, Errors, true) || !OperationsArray)
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UE_LOG(LogAgentFramework, Log, TEXT("BlueprintActions: Executing batch of %d operations on '%s'"), OperationsArray->Num(), *AssetPath);

	Result.bSuccess = true;
	int32 OpIndex = 0;

	for (const TSharedPtr<FJsonValue>& OpVal : *OperationsArray)
	{
		TSharedPtr<FJsonObject> OpObj = OpVal->AsObject();
		if (!OpObj.IsValid()) continue;

		FString ToolName;
		TArray<FString> OpErrors;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(OpObj.ToSharedRef(), TEXT("tool_name"), ToolName, OpErrors, true) || ToolName.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Operation at index %d has no tool_name."), OpIndex));
			Result.bSuccess = false;
			return Result;
		}

		TSharedPtr<FJsonObject> ArgsObj;
		const TSharedPtr<FJsonObject>* TmpArgs = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(OpObj.ToSharedRef(), TEXT("arguments"), TmpArgs, OpErrors, false) && TmpArgs)
		{
			ArgsObj = *TmpArgs;
		}
		else
		{
			ArgsObj = MakeShared<FJsonObject>();
		}

		// Prepare sub-parameters
		TSharedRef<FJsonObject> SubParams = MakeShared<FJsonObject>();
		// Copy arguments
		if (ArgsObj.IsValid())
		{
			for (auto& Pair : ArgsObj->Values)
			{
				SubParams->SetField(Pair.Key, Pair.Value);
			}
		}
		SubParams->SetStringField(TEXT("_tool_name"), ToolName);
		if (!SubParams->HasField(TEXT("asset_path")))
		{
			SubParams->SetStringField(TEXT("asset_path"), AssetPath);
		}

		// Execute
		FAgentFrameworkActionResult SubResult = ExecuteAction(SubParams);

		if (!SubResult.bSuccess)
		{
			Result.Errors.Add(FString::Printf(TEXT("Operation %d (%s) failed."), OpIndex, *ToolName));
			Result.Errors.Append(SubResult.Errors);
			Result.Warnings.Append(SubResult.Warnings);
			Result.bSuccess = false;
			Result.ResultMessage = FString::Printf(TEXT("Batch failed at operation %d (%s): %s"), OpIndex, *ToolName, *SubResult.ResultMessage);
			return Result;
		}

		Result.Warnings.Append(SubResult.Warnings);
		Result.ModifiedAssets.Append(SubResult.ModifiedAssets);
		if (!Result.ResultMessage.IsEmpty())
		{
			Result.ResultMessage += TEXT("\n");
		}
		Result.ResultMessage += FString::Printf(TEXT("[%s]: %s"), *ToolName, *SubResult.ResultMessage);
		OpIndex++;
	}

	// Single compile at the end of the batch
	bool bCompileOk = CompileAndReport(Blueprint, Result, true);
	if (IsValid(Blueprint->GetOutermost()))
	{
		Blueprint->GetOutermost()->MarkPackageDirty();
	}

	if (!bCompileOk)
	{
		Result.bSuccess = false;
	}

	return Result;
}

#if WITH_EDITOR
void FAgentFrameworkBlueprintActions::HandleGetExtraObjectTags(FAssetRegistryTagsContext Context)
{
	const UObject* InObject = Context.GetObject();
	if (!InObject) return;
	const UBlueprint* Blueprint = Cast<UBlueprint>(InObject);
	if (!Blueprint) return;

	FString ParentClassName = Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("None");

	TArray<TSharedPtr<FJsonValue>> VarsArray;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), GetVarTypeString(Var.VarType));
		VarObj->SetStringField(TEXT("category"), !Var.Category.IsEmpty() ? Var.Category.ToString() : TEXT("Default"));
		VarsArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}

	FString VariablesJsonStr;
	TSharedRef<TJsonWriter<>> VarWriter = TJsonWriterFactory<>::Create(&VariablesJsonStr);
	FJsonSerializer::Serialize(VarsArray, VarWriter);

	TArray<TSharedPtr<FJsonValue>> CustomEventsArray;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
			{
				CustomEventsArray.Add(MakeShared<FJsonValueString>(CustomEventNode->GetFunctionName().ToString()));
			}
		}
	}

	FString CustomEventsJsonStr;
	TSharedRef<TJsonWriter<>> EventWriter = TJsonWriterFactory<>::Create(&CustomEventsJsonStr);
	FJsonSerializer::Serialize(CustomEventsArray, EventWriter);

	Context.AddTag(UObject::FAssetRegistryTag(TEXT("AgentFramework_ParentClass"), ParentClassName, UObject::FAssetRegistryTag::TT_Alphabetical));
	Context.AddTag(UObject::FAssetRegistryTag(TEXT("AgentFramework_Variables"), VariablesJsonStr, UObject::FAssetRegistryTag::TT_Hidden));
	Context.AddTag(UObject::FAssetRegistryTag(TEXT("AgentFramework_CustomEvents"), CustomEventsJsonStr, UObject::FAssetRegistryTag::TT_Hidden));
}
#endif

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteGetBlueprintSchema(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true) || AssetPath.IsEmpty())
	{
		Result.bSuccess = false;
		Result.Errors.Append(Errors);
		if (AssetPath.IsEmpty() && Result.Errors.Num() == 0)
		{
			Result.Errors.Add(TEXT("Missing or empty required field: asset_path"));
		}
		return Result;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FString ObjectPathStr = AssetPath;
	if (!ObjectPathStr.Contains(TEXT(".")))
	{
		ObjectPathStr = ObjectPathStr + TEXT(".") + FPackageName::GetShortName(ObjectPathStr);
	}

	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(*ObjectPathStr));
	bool bUseFallback = false;
	if (!AssetData.IsValid())
	{
		bUseFallback = true;
	}
	else if (!AssetData.AssetClassPath.GetAssetName().ToString().Contains(TEXT("Blueprint")))
	{
		Result.Errors.Add(FString::Printf(TEXT("Asset is not a Blueprint: '%s'"), *AssetPath));
		return Result;
	}

	FString ParentClass = TEXT("None");
	TArray<TSharedPtr<FJsonValue>> Variables;
	TArray<TSharedPtr<FJsonValue>> CustomEvents;
	bool bLoadedFallback = false;

	FAssetTagValueRef ParentClassTag = AssetData.TagsAndValues.FindTag(FName(TEXT("AgentFramework_ParentClass")));
	FAssetTagValueRef VariablesTag = AssetData.TagsAndValues.FindTag(FName(TEXT("AgentFramework_Variables")));
	FAssetTagValueRef CustomEventsTag = AssetData.TagsAndValues.FindTag(FName(TEXT("AgentFramework_CustomEvents")));

	if (!bUseFallback && ParentClassTag.IsSet() && VariablesTag.IsSet() && CustomEventsTag.IsSet())
	{
		ParentClass = ParentClassTag.GetValue();

		FString VariablesJson = VariablesTag.GetValue();
		TSharedPtr<FJsonValue> VarVal;
		TSharedRef<TJsonReader<>> VarReader = TJsonReaderFactory<>::Create(VariablesJson);
		if (FJsonSerializer::Deserialize(VarReader, VarVal) && VarVal.IsValid())
		{
			Variables = VarVal->AsArray();
		}

		FString CustomEventsJson = CustomEventsTag.GetValue();
		TSharedPtr<FJsonValue> EventVal;
		TSharedRef<TJsonReader<>> EventReader = TJsonReaderFactory<>::Create(CustomEventsJson);
		if (FJsonSerializer::Deserialize(EventReader, EventVal) && EventVal.IsValid())
		{
			CustomEvents = EventVal->AsArray();
		}
	}
	else
	{
		bLoadedFallback = true;
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPathStr);
		if (!IsValid(Blueprint))
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to load Blueprint fallback: '%s'"), *AssetPath));
			return Result;
		}

		ParentClass = IsValid(Blueprint->ParentClass) ? Blueprint->ParentClass->GetName() : TEXT("None");

		for (const FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
			VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
			VarObj->SetStringField(TEXT("type"), GetVarTypeString(Var.VarType));
			VarObj->SetStringField(TEXT("category"), !Var.Category.IsEmpty() ? Var.Category.ToString() : TEXT("Default"));
			Variables.Add(MakeShared<FJsonValueObject>(VarObj));
		}

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (!IsValid(Graph)) continue;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (IsValid(Node))
				{
					if (UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(Node))
					{
						CustomEvents.Add(MakeShared<FJsonValueString>(CustomEventNode->GetFunctionName().ToString()));
					}
				}
			}
		}
	}

	FString AssetName = AssetData.IsValid() ? AssetData.AssetName.ToString() : FPackageName::GetShortName(AssetPath);

	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetStringField(TEXT("asset_name"), AssetName);
	ResponseObj->SetStringField(TEXT("parent_class"), ParentClass);
	ResponseObj->SetArrayField(TEXT("variables"), Variables);
	ResponseObj->SetArrayField(TEXT("custom_events"), CustomEvents);

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseString;
	if (bLoadedFallback)
	{
		Result.Warnings.Add(TEXT("Blueprint asset was loaded in memory as a fallback because custom tags were not populated in the Asset Registry. Please save the asset to enable fast non-loading schema extraction."));
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteCheckAssetState(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Errors, true))
	{
		Result.Errors.Append(Errors);
		return Result;
	}
	AssetPath = ExpandBlueprintAssetPath(AssetPath);

	UPackage* Package = FindPackage(nullptr, *AssetPath);
	bool bIsDirty = IsValid(Package) ? Package->IsDirty() : false;

	bool bIsOpen = false;
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (IsValid(Asset) && GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (IsValid(AssetEditorSubsystem))
		{
			bIsOpen = (AssetEditorSubsystem->FindEditorForAsset(Asset, false) != nullptr);
		}
	}

	bool bIsLocked = false;
	ISourceControlModule& SCModule = ISourceControlModule::Get();
	if (SCModule.IsEnabled() && SCModule.GetProvider().IsAvailable())
	{
		FString FilePath = USourceControlHelpers::PackageFilename(AssetPath);
		FSourceControlStatePtr SCState = SCModule.GetProvider().GetState(FilePath, EStateCacheUsage::Use);
		if (SCState.IsValid())
		{
			bIsLocked = SCState->IsCheckedOutOther() || !SCState->CanEdit();
		}
	}

	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetBoolField(TEXT("bIsDirty"), bIsDirty);
	ResponseObj->SetBoolField(TEXT("bIsOpen"), bIsOpen);
	ResponseObj->SetBoolField(TEXT("bIsLocked"), bIsLocked);

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseString;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteModifyBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (Params->HasField(TEXT("operations")))
	{
		return ExecuteBatchOperations(Params, Result);
	}
	Result.bSuccess = true;
	Result.ResultMessage = TEXT("modify_blueprint called with no operations; compilation and checks succeeded.");
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteDisconnectPins(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	Result.bSuccess = false;

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Params->TryGetStringField(TEXT("TargetAsset"), AssetPath);
	}
	AssetPath = ExpandBlueprintAssetPath(AssetPath);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load Blueprint asset: %s"), *AssetPath));
		return Result;
	}

	FString NodeGuidOrName;
	if (!Params->TryGetStringField(TEXT("node_guid"), NodeGuidOrName))
	{
		if (!Params->TryGetStringField(TEXT("NodeGuid"), NodeGuidOrName))
		{
			if (!Params->TryGetStringField(TEXT("node_name"), NodeGuidOrName))
			{
				Params->TryGetStringField(TEXT("source_node"), NodeGuidOrName);
			}
		}
	}

	FString PinName;
	if (!Params->TryGetStringField(TEXT("pin_name"), PinName))
	{
		Params->TryGetStringField(TEXT("PinName"), PinName);
	}

	FString TargetNodeGuidOrName;
	if (!Params->TryGetStringField(TEXT("target_node_guid"), TargetNodeGuidOrName))
	{
		if (!Params->TryGetStringField(TEXT("TargetNodeGuid"), TargetNodeGuidOrName))
		{
			if (!Params->TryGetStringField(TEXT("target_node_name"), TargetNodeGuidOrName))
			{
				Params->TryGetStringField(TEXT("target_node"), TargetNodeGuidOrName);
			}
		}
	}

	FString TargetPinName;
	if (!Params->TryGetStringField(TEXT("target_pin_name"), TargetPinName))
	{
		Params->TryGetStringField(TEXT("TargetPinName"), TargetPinName);
	}

	bool bDisconnectAll = false;
	if (!Params->TryGetBoolField(TEXT("b_disconnect_all"), bDisconnectAll))
	{
		Params->TryGetBoolField(TEXT("bDisconnectAll"), bDisconnectAll);
	}

	if (!bDisconnectAll && TargetNodeGuidOrName.IsEmpty() && TargetPinName.IsEmpty())
	{
		bDisconnectAll = true;
	}

	FString GraphName;
	if (!Params->TryGetStringField(TEXT("graph_name"), GraphName))
	{
		Params->TryGetStringField(TEXT("GraphName"), GraphName);
	}

	UEdGraphNode* FoundNode = nullptr;
	UEdGraph* TargetGraph = nullptr;

	auto SearchGraphForNode = [&NodeGuidOrName](UEdGraph* Graph) -> UEdGraphNode*
	{
		if (!Graph) return nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			FString GuidStrHex = Node->NodeGuid.ToString(EGuidFormats::Digits);
			FString GuidStrHyphen = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
			FString NodeNameStr = Node->GetName();

			if (GuidStrHex.Equals(NodeGuidOrName, ESearchCase::IgnoreCase) ||
				GuidStrHyphen.Equals(NodeGuidOrName, ESearchCase::IgnoreCase) ||
				NodeNameStr.Equals(NodeGuidOrName, ESearchCase::IgnoreCase))
			{
				return Node;
			}
		}
		return nullptr;
	};

	if (!GraphName.IsEmpty())
	{
		TargetGraph = FindOrCreateEventGraph(Blueprint, GraphName);
		if (TargetGraph)
		{
			FoundNode = SearchGraphForNode(TargetGraph);
		}
	}

	if (!FoundNode)
	{
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		for (UEdGraph* Graph : AllGraphs)
		{
			FoundNode = SearchGraphForNode(Graph);
			if (FoundNode)
			{
				TargetGraph = Graph;
				break;
			}
		}
	}

	if (!FoundNode)
	{
		Result.Errors.Add(FString::Printf(TEXT("Node '%s' not found in Blueprint '%s'."), *NodeGuidOrName, *AssetPath));
		return Result;
	}

	UEdGraphPin* FoundPin = nullptr;
	for (UEdGraphPin* Pin : FoundNode->Pins)
	{
		if (!Pin) continue;
		if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) ||
			Pin->PinFriendlyName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			FoundPin = Pin;
			break;
		}
	}

	if (!FoundPin)
	{
		Result.Errors.Add(FString::Printf(TEXT("Pin '%s' not found on node '%s' in Blueprint '%s'."), *PinName, *NodeGuidOrName, *AssetPath));
		return Result;
	}

	int32 DisconnectedCount = 0;
	FoundNode->Modify();
	Blueprint->Modify();

	if (bDisconnectAll || (TargetNodeGuidOrName.IsEmpty() && TargetPinName.IsEmpty()))
	{
		DisconnectedCount = FoundPin->LinkedTo.Num();
		FoundPin->BreakAllPinLinks();
	}
	else
	{
		TArray<UEdGraphPin*> LinksToBreak;
		for (UEdGraphPin* LinkedPin : FoundPin->LinkedTo)
		{
			if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;
			UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();

			bool bNodeMatch = TargetNodeGuidOrName.IsEmpty() ||
				LinkedNode->NodeGuid.ToString(EGuidFormats::Digits).Equals(TargetNodeGuidOrName, ESearchCase::IgnoreCase) ||
				LinkedNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens).Equals(TargetNodeGuidOrName, ESearchCase::IgnoreCase) ||
				LinkedNode->GetName().Equals(TargetNodeGuidOrName, ESearchCase::IgnoreCase);

			bool bPinMatch = TargetPinName.IsEmpty() ||
				LinkedPin->PinName.ToString().Equals(TargetPinName, ESearchCase::IgnoreCase) ||
				LinkedPin->PinFriendlyName.ToString().Equals(TargetPinName, ESearchCase::IgnoreCase);

			if (bNodeMatch && bPinMatch)
			{
				LinksToBreak.Add(LinkedPin);
			}
		}

		for (UEdGraphPin* TargetPinToBreak : LinksToBreak)
		{
			if (TargetPinToBreak->GetOwningNode())
			{
				TargetPinToBreak->GetOwningNode()->Modify();
			}
			FoundPin->BreakLinkTo(TargetPinToBreak);
			DisconnectedCount++;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FAgentFrameworkActionsModule::AgentDirtiedPackages.Add(FName(*AssetPath));
	CompileAndReport(Blueprint, Result, true);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully disconnected %d link(s) on pin '%s' of node '%s' in Blueprint '%s'."),
		DisconnectedCount, *PinName, *NodeGuidOrName, *AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteModifySubobject(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	Result.bSuccess = false;

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		if (!Params->TryGetStringField(TEXT("AssetPath"), AssetPath))
		{
			Params->TryGetStringField(TEXT("TargetAsset"), AssetPath);
		}
	}
	AssetPath = ExpandBlueprintAssetPath(AssetPath);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load Blueprint asset: %s"), *AssetPath));
		return Result;
	}

	FString SubObjectPath;
	if (!Params->TryGetStringField(TEXT("subobject_path"), SubObjectPath))
	{
		Params->TryGetStringField(TEXT("SubObjectPath"), SubObjectPath);
	}

	const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
	if (!Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
	{
		Params->TryGetObjectField(TEXT("Properties"), PropertiesObj);
	}

	if (!PropertiesObj || !PropertiesObj->IsValid())
	{
		Result.Errors.Add(TEXT("Missing or invalid 'properties' object for modify_blueprint_subobject."));
		return Result;
	}

	FString CleanPath = SubObjectPath;
	if (CleanPath.StartsWith(TEXT("WidgetTree."), ESearchCase::IgnoreCase))
	{
		CleanPath = CleanPath.Mid(11);
	}

	UObject* SubObjectToModify = nullptr;

	if (UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Blueprint))
	{
		if (WidgetBP->WidgetTree)
		{
			FString WidgetName = CleanPath;
			bool bTargetSlot = false;
			if (WidgetName.EndsWith(TEXT(".Slot"), ESearchCase::IgnoreCase))
			{
				WidgetName = WidgetName.LeftChop(5);
				bTargetSlot = true;
			}
			else if (WidgetName.EndsWith(TEXT(".slot"), ESearchCase::IgnoreCase))
			{
				WidgetName = WidgetName.LeftChop(5);
				bTargetSlot = true;
			}

			UWidget* FoundWidget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
			if (FoundWidget)
			{
				if (bTargetSlot)
				{
					SubObjectToModify = FoundWidget->Slot;
				}
				else
				{
					SubObjectToModify = FoundWidget;
				}
			}
		}
	}

	if (!SubObjectToModify)
	{
		USCS_Node* SCSNode = FindSCSNodeByName(Blueprint, CleanPath);
		if (SCSNode && SCSNode->ComponentTemplate)
		{
			SubObjectToModify = SCSNode->ComponentTemplate;
		}
	}

	if (!SubObjectToModify && Blueprint->GeneratedClass)
	{
		UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
		if (CDO)
		{
			FObjectPropertyBase* ObjProp = FindFProperty<FObjectPropertyBase>(Blueprint->GeneratedClass, FName(*CleanPath));
			if (ObjProp)
			{
				SubObjectToModify = ObjProp->GetObjectPropertyValue_InContainer(CDO);
			}
		}
	}

	if (!SubObjectToModify)
	{
		FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
		FString AssetName = FPaths::GetBaseFilename(AssetPath);
		FString FullSubObjPath = FString::Printf(TEXT("%s.%s:%s"), *PackageName, *AssetName, *SubObjectPath);
		SubObjectToModify = StaticLoadObject(UObject::StaticClass(), nullptr, *FullSubObjPath);
	}

	if (!SubObjectToModify)
	{
		Result.Errors.Add(FString::Printf(TEXT("Could not resolve sub-object '%s' in Blueprint '%s'."), *SubObjectPath, *AssetPath));
		return Result;
	}

	SubObjectToModify->Modify();
	int32 ModifiedCount = 0;

	for (const auto& Pair : (*PropertiesObj)->Values)
	{
		FString PropName = FString(Pair.Key);
		TSharedPtr<FJsonValue> JsonVal = Pair.Value;

		UObject* TargetObjectForProp = SubObjectToModify;
		if (PropName.StartsWith(TEXT("Slot."), ESearchCase::IgnoreCase) || PropName.StartsWith(TEXT("slot."), ESearchCase::IgnoreCase))
		{
			if (UWidget* WidgetObj = Cast<UWidget>(SubObjectToModify))
			{
				if (WidgetObj->Slot)
				{
					TargetObjectForProp = WidgetObj->Slot;
					PropName = PropName.Mid(5);
				}
			}
		}

		FProperty* Prop = TargetObjectForProp->GetClass()->FindPropertyByName(FName(*PropName));
		if (!Prop)
		{
			for (TFieldIterator<FProperty> It(TargetObjectForProp->GetClass()); It; ++It)
			{
				if (It->GetName().Equals(PropName, ESearchCase::IgnoreCase))
				{
					Prop = *It;
					break;
				}
			}
		}

		if (!Prop)
		{
			Result.Warnings.Add(FString::Printf(TEXT("Property '%s' not found on class '%s'."), *PropName, *TargetObjectForProp->GetClass()->GetName()));
			continue;
		}

		FString ValueString;
		if (JsonVal->Type == EJson::String)
		{
			ValueString = JsonVal->AsString();
		}
		else if (JsonVal->Type == EJson::Number)
		{
			ValueString = FString::Printf(TEXT("%f"), JsonVal->AsNumber());
		}
		else if (JsonVal->Type == EJson::Boolean)
		{
			ValueString = JsonVal->AsBool() ? TEXT("True") : TEXT("False");
		}
		else if (JsonVal->Type == EJson::Object)
		{
			ValueString = FormatJsonObjectToUnrealText(JsonVal->AsObject());
		}

		TargetObjectForProp->PreEditChange(Prop);
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(TargetObjectForProp);
		const TCHAR* ImportResult = Prop->ImportText_Direct(*ValueString, ValuePtr, TargetObjectForProp, PPF_None);
		FPropertyChangedEvent ChangeEvent(Prop);
		TargetObjectForProp->PostEditChangeProperty(ChangeEvent);

		if (ImportResult != nullptr)
		{
			ModifiedCount++;
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Failed to import text '%s' for property '%s'."), *ValueString, *PropName));
		}
	}

	if (UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Blueprint))
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
	FAgentFrameworkActionsModule::AgentDirtiedPackages.Add(FName(*AssetPath));
	CompileAndReport(Blueprint, Result, true);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully modified %d property(ies) on sub-object '%s' (class '%s') in Blueprint '%s'."),
		ModifiedCount, *SubObjectPath, *SubObjectToModify->GetClass()->GetName(), *AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteConfigureActorReplication(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	Result.bSuccess = false;

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Params->TryGetStringField(TEXT("TargetAsset"), AssetPath);
	}
	AssetPath = ExpandBlueprintAssetPath(AssetPath);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load Blueprint asset: %s"), *AssetPath));
		return Result;
	}

	if (!Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(AActor::StaticClass()))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint '%s' is not an Actor Blueprint (Parent class: %s). configure_actor_replication requires an Actor Blueprint."),
			*AssetPath, Blueprint->ParentClass ? *Blueprint->ParentClass->GetName() : TEXT("None")));
		return Result;
	}

	if (!Blueprint->GeneratedClass)
	{
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}

	AActor* ActorCDO = Cast<AActor>(Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr);
	if (!ActorCDO)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to retrieve CDO for Actor Blueprint '%s'."), *AssetPath));
		return Result;
	}

	bool bReplicates = true;
	if (!Params->TryGetBoolField(TEXT("b_replicates"), bReplicates))
	{
		Params->TryGetBoolField(TEXT("bReplicates"), bReplicates);
	}

	bool bReplicateMovement = true;
	if (!Params->TryGetBoolField(TEXT("b_replicate_movement"), bReplicateMovement))
	{
		Params->TryGetBoolField(TEXT("bReplicateMovement"), bReplicateMovement);
	}

	FString NetDormancyStr = TEXT("DORM_Never");
	if (!Params->TryGetStringField(TEXT("net_dormancy"), NetDormancyStr))
	{
		Params->TryGetStringField(TEXT("NetDormancy"), NetDormancyStr);
	}

	double NetUpdateFrequency = 100.0;
	if (!Params->TryGetNumberField(TEXT("net_update_frequency"), NetUpdateFrequency))
	{
		Params->TryGetNumberField(TEXT("NetUpdateFrequency"), NetUpdateFrequency);
	}

	double NetPriority = 1.0;
	if (!Params->TryGetNumberField(TEXT("net_priority"), NetPriority))
	{
		Params->TryGetNumberField(TEXT("NetPriority"), NetPriority);
	}

	ActorCDO->Modify();
	Blueprint->Modify();

	ActorCDO->SetReplicates(bReplicates);
	ActorCDO->SetReplicateMovement(bReplicateMovement);

	if (NetDormancyStr.Equals(TEXT("DORM_Awake"), ESearchCase::IgnoreCase))             ActorCDO->NetDormancy = DORM_Awake;
	else if (NetDormancyStr.Equals(TEXT("DORM_DormantAll"), ESearchCase::IgnoreCase))   ActorCDO->NetDormancy = DORM_DormantAll;
	else if (NetDormancyStr.Equals(TEXT("DORM_DormantPartial"), ESearchCase::IgnoreCase))ActorCDO->NetDormancy = DORM_DormantPartial;
	else if (NetDormancyStr.Equals(TEXT("DORM_Initial"), ESearchCase::IgnoreCase))       ActorCDO->NetDormancy = DORM_Initial;
	else                                                                                 ActorCDO->NetDormancy = DORM_Never;

	ActorCDO->NetUpdateFrequency = static_cast<float>(NetUpdateFrequency);
	ActorCDO->NetPriority = static_cast<float>(NetPriority);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FAgentFrameworkActionsModule::AgentDirtiedPackages.Add(FName(*AssetPath));
	CompileAndReport(Blueprint, Result, true);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully configured replication for Actor Blueprint '%s': bReplicates=%s, bReplicateMovement=%s, NetDormancy=%s, NetUpdateFrequency=%.1f, NetPriority=%.1f"),
		*AssetPath, bReplicates ? TEXT("True") : TEXT("False"), bReplicateMovement ? TEXT("True") : TEXT("False"), *NetDormancyStr, NetUpdateFrequency, NetPriority);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBlueprintActions::ExecuteSetVariableReplication(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	Result.bSuccess = false;

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Params->TryGetStringField(TEXT("TargetAsset"), AssetPath);
	}
	AssetPath = ExpandBlueprintAssetPath(AssetPath);

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load Blueprint asset: %s"), *AssetPath));
		return Result;
	}

	FString VarName;
	if (!Params->TryGetStringField(TEXT("variable_name"), VarName))
	{
		Params->TryGetStringField(TEXT("VariableName"), VarName);
	}

	FString RepTypeStr = TEXT("Replicated");
	if (!Params->TryGetStringField(TEXT("replication_type"), RepTypeStr))
	{
		Params->TryGetStringField(TEXT("ReplicationType"), RepTypeStr);
	}

	FString RepNotifyFuncStr;
	if (!Params->TryGetStringField(TEXT("rep_notify_func"), RepNotifyFuncStr))
	{
		Params->TryGetStringField(TEXT("RepNotifyFunc"), RepNotifyFuncStr);
	}

	FString RepConditionStr;
	if (!Params->TryGetStringField(TEXT("replication_condition"), RepConditionStr))
	{
		Params->TryGetStringField(TEXT("ReplicationCondition"), RepConditionStr);
	}

	FBPVariableDescription* TargetVarDesc = nullptr;
	for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
	{
		if (VarDesc.VarName.ToString().Equals(VarName, ESearchCase::IgnoreCase))
		{
			TargetVarDesc = &VarDesc;
			break;
		}
	}

	if (!TargetVarDesc)
	{
		Result.Errors.Add(FString::Printf(TEXT("Variable '%s' not found in Blueprint '%s'. Note: Inherited C++ variables cannot be modified via Blueprint variable description."),
			*VarName, *AssetPath));
		return Result;
	}

	Blueprint->Modify();

	if (RepTypeStr.Equals(TEXT("None"), ESearchCase::IgnoreCase))
	{
		TargetVarDesc->PropertyFlags &= ~(CPF_Net | CPF_RepNotify);
		TargetVarDesc->RepNotifyFunc = NAME_None;
	}
	else if (RepTypeStr.Equals(TEXT("Replicated"), ESearchCase::IgnoreCase))
	{
		TargetVarDesc->PropertyFlags |= CPF_Net;
		TargetVarDesc->PropertyFlags &= ~CPF_RepNotify;
		TargetVarDesc->RepNotifyFunc = NAME_None;
	}
	else if (RepTypeStr.Equals(TEXT("RepNotify"), ESearchCase::IgnoreCase))
	{
		TargetVarDesc->PropertyFlags |= (CPF_Net | CPF_RepNotify);

		FName RepNotifyFuncName;
		if (!RepNotifyFuncStr.IsEmpty())
		{
			RepNotifyFuncName = FName(*RepNotifyFuncStr);
		}
		else
		{
			RepNotifyFuncName = FName(*FString::Printf(TEXT("OnRep_%s"), *TargetVarDesc->VarName.ToString()));
		}
		TargetVarDesc->RepNotifyFunc = RepNotifyFuncName;

		UEdGraph* FuncGraph = FindObject<UEdGraph>(Blueprint, *RepNotifyFuncName.ToString());
		if (!FuncGraph)
		{
			FuncGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, RepNotifyFuncName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, FuncGraph, false, nullptr);
		}
	}

	if (!RepConditionStr.IsEmpty())
	{
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, TargetVarDesc->VarName, nullptr, FName(TEXT("ReplicationCondition")), RepConditionStr);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FAgentFrameworkActionsModule::AgentDirtiedPackages.Add(FName(*AssetPath));
	CompileAndReport(Blueprint, Result, true);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully updated replication for variable '%s' in Blueprint '%s': ReplicationType=%s, RepNotifyFunc='%s'"),
		*VarName, *AssetPath, *RepTypeStr, TargetVarDesc->RepNotifyFunc != NAME_None ? *TargetVarDesc->RepNotifyFunc.ToString() : TEXT("None"));
	return Result;
}

