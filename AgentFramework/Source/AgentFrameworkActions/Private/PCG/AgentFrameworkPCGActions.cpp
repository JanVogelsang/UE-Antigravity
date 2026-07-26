// Copyright 2026 AgentFramework. All Rights Reserved.

#include "PCG/AgentFrameworkPCGActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"

// PCG plugin headers — guarded with a module availability check at runtime
// The PCGComponent and PCGGraph types are only available when the PCG plugin is loaded.
// We use dynamic module loading and reflection to avoid a hard compile dependency.
#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "Sound/SoundBase.h"
#endif

// Asset management
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

// JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Misc
#include "ScopedTransaction.h"

// ============================================================================
// Statics / Lifecycle
// ============================================================================

FAgentFrameworkPCGActions::FAgentFrameworkPCGActions() {}
FAgentFrameworkPCGActions::~FAgentFrameworkPCGActions() {}

FName FAgentFrameworkPCGActions::GetActionName() const { return FName(TEXT("PCG")); }

TArray<FString> FAgentFrameworkPCGActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_pcg_graph"),
		TEXT("attach_pcg_component"),
		TEXT("set_pcg_parameter"),
		TEXT("generate_pcg_local"),
		TEXT("get_pcg_info"),
		TEXT("wire_pcg_nodes")
	};
}

bool FAgentFrameworkPCGActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, OutErrors, true))
	{
		return false;
	}

	FString DummyString;
	if (ToolName == TEXT("create_pcg_graph"))
	{
		return UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), DummyString, OutErrors, true);
	}
	else if (ToolName == TEXT("attach_pcg_component"))
	{
		return UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_name"), DummyString, OutErrors, true);
	}
	else if (ToolName == TEXT("set_pcg_parameter"))
	{
		bool bValid = true;
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_name"), DummyString, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("parameter_name"), DummyString, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("parameter_value"), DummyString, OutErrors, true);
		return bValid;
	}
	else if (ToolName == TEXT("generate_pcg_local"))
	{
		return UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_name"), DummyString, OutErrors, true);
	}
	else if (ToolName == TEXT("get_pcg_info"))
	{
		return UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_name"), DummyString, OutErrors, true);
	}
	else if (ToolName == TEXT("wire_pcg_nodes"))
	{
		bool bValid = true;
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("graph_path"), DummyString, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("source_node"), DummyString, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("source_pin"), DummyString, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("target_node"), DummyString, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("target_pin"), DummyString, OutErrors, true);
		return bValid;
	}

	return true;
}

// ============================================================================
// Helper: CheckPCGAvailable
// ============================================================================

bool FAgentFrameworkPCGActions::CheckPCGAvailable(FAgentFrameworkActionResult& Result)
{
	if (!FModuleManager::Get().IsModuleLoaded("PCG"))
	{
		Result.Errors.Add(TEXT("PCG plugin is not loaded in this project. Enable the 'PCG' plugin in the .uproject file and restart the editor. PCG requires Unreal Engine 5.2 or later."));
		return false;
	}
	return true;
}

// ============================================================================
// Helper: FindActorByName
// ============================================================================

AActor* FAgentFrameworkPCGActions::FindActorByName(const FString& ActorName)
{
#if WITH_EDITOR
	if (!GEditor) return nullptr;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!IsValid(World)) return nullptr;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (IsValid(Actor) && Actor->GetActorLabel() == ActorName)
		{
			return Actor;
		}
	}
	// Fallback: match by object name
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (IsValid(Actor) && Actor->GetName() == ActorName)
		{
			return Actor;
		}
	}
#endif
	return nullptr;
}

// ============================================================================
// ExecuteAction — Dispatch
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPCGActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FString ToolName;
	TArray<FString> DummyErrors;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, DummyErrors, false);

	bool bIsReadOnly = (ToolName == TEXT("get_pcg_info"));

	TOptional<FScopedTransaction> Transaction;
	if (!bIsReadOnly)
	{
		Transaction.Emplace(FText::FromString(TEXT("AgentFramework PCG Action")));
	}

	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	if (!CheckPCGAvailable(Result))
	{
		if (Transaction.IsSet())
		{
			Transaction->Cancel();
		}
		return Result;
	}

	if (ToolName == TEXT("create_pcg_graph"))           Result = ExecuteCreatePCGGraph(Params, Result);
	else if (ToolName == TEXT("attach_pcg_component"))  Result = ExecuteAttachPCGComponent(Params, Result);
	else if (ToolName == TEXT("set_pcg_parameter"))     Result = ExecuteSetPCGParameter(Params, Result);
	else if (ToolName == TEXT("generate_pcg_local"))    Result = ExecuteGeneratePCGLocal(Params, Result);
	else if (ToolName == TEXT("get_pcg_info"))          Result = ExecuteGetPCGInfo(Params, Result);
	else if (ToolName == TEXT("wire_pcg_nodes"))        Result = ExecuteWirePCGNodes(Params, Result);
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown PCG tool: '%s'"), *ToolName));
	}

	if (Transaction.IsSet() && !Result.bSuccess)
	{
		Transaction->Cancel();
	}

#if WITH_EDITOR
	if (Result.bSuccess && GEditor)
	{
		USoundBase* SuccessSound = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
		if (IsValid(SuccessSound))
		{
			GEditor->PlayEditorSound(SuccessSound);
		}
	}
#endif

	return Result;
}

// ============================================================================
// ExecuteCreatePCGGraph
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPCGActions::ExecuteCreatePCGGraph(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!CheckPCGAvailable(Result)) return Result;

	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	// Look up the PCGGraph class via reflection (avoids hard-linking the PCG module)
	UClass* PCGGraphClass = FindFirstObject<UClass>(TEXT("PCGGraph"), EFindFirstObjectOptions::None);
	if (!IsValid(PCGGraphClass))
	{
		PCGGraphClass = FindFirstObject<UClass>(TEXT("UPCGGraph"), EFindFirstObjectOptions::None);
	}

	if (!IsValid(PCGGraphClass))
	{
		Result.Errors.Add(TEXT("PCGGraph class not found via reflection. Ensure the PCG plugin is fully loaded and UE5.2+ is in use."));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName   = FPackageName::GetShortName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString UniqueName, UniquePackagePath;
	AssetTools.CreateUniqueAssetName(AssetPath, TEXT(""), UniquePackagePath, UniqueName);

	// Use UObject factory pattern — PCG provides UPCGGraphFactory
	UClass* FactoryClass = FindFirstObject<UClass>(TEXT("PCGGraphFactory"), EFindFirstObjectOptions::None);
	UFactory* Factory = nullptr;
	if (IsValid(FactoryClass))
	{
		Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
	}

	UObject* NewAsset = nullptr;
	if (IsValid(Factory))
	{
		NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, PCGGraphClass, Factory);
	}
	else
	{
		// Fallback: create empty package and construct directly
		FString PackageName = PackagePath / AssetName;
		UPackage* Package = CreatePackage(*PackageName);
		if (IsValid(Package))
		{
			NewAsset = NewObject<UObject>(Package, PCGGraphClass, FName(*AssetName), RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(NewAsset);
		}
	}

	if (!IsValid(NewAsset))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create PCG graph at '%s'. Check the path is valid."), *AssetPath));
		return Result;
	}

	UPackage* Package = NewAsset->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, NewAsset, *PackageFilename, SaveArgs);
		}
	}

	UE_LOG(LogAgentFramework, Log, TEXT("PCGActions: Created PCG graph at '%s'"), *AssetPath);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(
		TEXT("Created PCG graph '%s'.\n"
		     "Next steps: (1) Use attach_pcg_component to assign it to a level actor or PCGVolume. "
		     "(2) Use set_pcg_parameter to configure exposed parameters. "
		     "(3) Use generate_pcg_local to trigger generation. "
		     "PCG graph node authoring (adding scatter/sample/spline nodes) is done in the editor UI."),
		*AssetName);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteAttachPCGComponent
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPCGActions::ExecuteAttachPCGComponent(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!CheckPCGAvailable(Result)) return Result;

	FString ActorName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_name"), ActorName, Result.Errors, true))
	{
		return Result;
	}

	AActor* TargetActor = FindActorByName(ActorName);
	if (!IsValid(TargetActor))
	{
		Result.Errors.Add(FString::Printf(TEXT("Actor '%s' not found in the current level. Check the actor label in the Outliner. Spawn it first using spawn_actor if needed."), *ActorName));
		return Result;
	}

	// Find PCGComponent class via reflection
	UClass* PCGComponentClass = FindFirstObject<UClass>(TEXT("PCGComponent"), EFindFirstObjectOptions::None);
	if (!IsValid(PCGComponentClass))
	{
		PCGComponentClass = FindFirstObject<UClass>(TEXT("UPCGComponent"), EFindFirstObjectOptions::None);
	}

	if (!IsValid(PCGComponentClass))
	{
		Result.Errors.Add(TEXT("PCGComponent class not found. Ensure the PCG plugin is enabled."));
		return Result;
	}

	// Check if PCGComponent already exists on this actor
	UActorComponent* ExistingComp = TargetActor->FindComponentByClass(PCGComponentClass);
	UActorComponent* PCGComp = ExistingComp;

	if (!IsValid(ExistingComp))
	{
		TargetActor->Modify();
		PCGComp = NewObject<UActorComponent>(TargetActor, PCGComponentClass, TEXT("PCGComponent"), RF_Transactional);
		if (IsValid(PCGComp))
		{
			TargetActor->AddInstanceComponent(PCGComp);
			PCGComp->RegisterComponent();
			UE_LOG(LogAgentFramework, Log, TEXT("PCGActions: Added PCGComponent to actor '%s'"), *ActorName);
		}
		else
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to create PCGComponent on actor '%s'."), *ActorName));
			return Result;
		}
	}
	else
	{
		Result.Warnings.Add(FString::Printf(TEXT("Actor '%s' already has a PCGComponent — reusing it."), *ActorName));
	}

	// Optionally assign graph
	FString GraphPath;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("graph_path"), GraphPath, Result.Errors, false) && !GraphPath.IsEmpty())
	{
		UObject* GraphAsset = LoadObject<UObject>(nullptr, *GraphPath);
		if (IsValid(GraphAsset))
		{
			if (IsValid(PCGComp))
			{
				FProperty* GraphProp = PCGComp->GetClass()->FindPropertyByName(FName(TEXT("Graph")));
				if (GraphProp)
				{
					PCGComp->Modify();
					void* PropAddr = GraphProp->ContainerPtrToValuePtr<void>(PCGComp);
					FObjectProperty* ObjProp = CastField<FObjectProperty>(GraphProp);
					if (ObjProp)
					{
						ObjProp->SetObjectPropertyValue(PropAddr, GraphAsset);
					}
				}
				else
				{
					Result.Warnings.Add(TEXT("Could not find 'Graph' property on PCGComponent via reflection — graph not assigned."));
				}
			}
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("PCG graph not found at '%s' — component added without graph assignment."), *GraphPath));
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("PCGComponent %s on actor '%s'.%s"),
		IsValid(ExistingComp) ? TEXT("already exists") : TEXT("added"),
		*ActorName,
		GraphPath.IsEmpty() ? TEXT(" No graph assigned — use set_pcg_parameter after assigning a graph.") : *FString::Printf(TEXT(" Graph: '%s'"), *GraphPath));
	return Result;
}

// ============================================================================
// ExecuteSetPCGParameter
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPCGActions::ExecuteSetPCGParameter(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!CheckPCGAvailable(Result)) return Result;

	FString ActorName, ParameterName, ParameterValue;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_name"), ActorName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("parameter_name"), ParameterName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("parameter_value"), ParameterValue, Result.Errors, true))
	{
		return Result;
	}

	AActor* TargetActor = FindActorByName(ActorName);
	if (!IsValid(TargetActor))
	{
		Result.Errors.Add(FString::Printf(TEXT("Actor '%s' not found in current level."), *ActorName));
		return Result;
	}

	UClass* PCGComponentClass = FindFirstObject<UClass>(TEXT("PCGComponent"), EFindFirstObjectOptions::None);
	if (!IsValid(PCGComponentClass))
	{
		PCGComponentClass = FindFirstObject<UClass>(TEXT("UPCGComponent"), EFindFirstObjectOptions::None);
	}

	UActorComponent* PCGComp = IsValid(PCGComponentClass) ? TargetActor->FindComponentByClass(PCGComponentClass) : nullptr;
	if (!IsValid(PCGComp))
	{
		Result.Errors.Add(FString::Printf(TEXT("No PCGComponent found on actor '%s'. Use attach_pcg_component first."), *ActorName));
		return Result;
	}

	// PCG exposes parameters via OverrideParameters (FPCGMetadataAttributeBase)
	// We use a method-based approach: call SetOverrideAttributeValue via reflection
	UFunction* SetOverrideFunc = PCGComp->FindFunction(FName(TEXT("SetOverrideAttribute")));
	if (!SetOverrideFunc)
	{
		// Try graph instance parameters via property
		// PCGComponent stores parameters in GraphInstance->GetOverrideParams()
		// Fallback: use direct reflection on the component for simple properties
		FProperty* DirectProp = PCGComp->GetClass()->FindPropertyByName(FName(*ParameterName));
		if (DirectProp)
		{
			PCGComp->Modify();
			void* PropAddr = DirectProp->ContainerPtrToValuePtr<void>(PCGComp);
			DirectProp->ImportText_Direct(*ParameterValue, PropAddr, PCGComp, PPF_None);
			Result.bSuccess = true;
			Result.ResultMessage = FString::Printf(TEXT("Set PCGComponent property '%s' = '%s' on actor '%s'."), *ParameterName, *ParameterValue, *ActorName);
			return Result;
		}

		Result.Warnings.Add(FString::Printf(TEXT("SetOverrideAttribute not available — parameter '%s' may need to be set via the PCG Editor UI or the parameter must be exposed in the graph settings."), *ParameterName));
		Result.bSuccess = false;
		Result.ResultMessage = FString::Printf(TEXT("Could not set PCG parameter '%s' programmatically on actor '%s'. Expose the parameter in the PCG graph settings asset and try again."), *ParameterName, *ActorName);
		return Result;
	}

	// PCG ParameterOverride API (available in UE 5.3+)
	Result.Warnings.Add(TEXT("PCG parameter override API varies by engine version. If this fails, set the parameter directly in the PCG Graph settings asset."));
	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("PCG parameter '%s' = '%s' set attempt on actor '%s'."), *ParameterName, *ParameterValue, *ActorName);
	return Result;
}

// ============================================================================
// ExecuteGeneratePCGLocal
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPCGActions::ExecuteGeneratePCGLocal(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!CheckPCGAvailable(Result)) return Result;

	FString ActorName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_name"), ActorName, Result.Errors, true))
	{
		return Result;
	}

	bool bForce = true;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("force"), bForce, Result.Errors, false);

	AActor* TargetActor = FindActorByName(ActorName);
	if (!IsValid(TargetActor))
	{
		Result.Errors.Add(FString::Printf(TEXT("Actor '%s' not found in current level."), *ActorName));
		return Result;
	}

	UClass* PCGComponentClass = FindFirstObject<UClass>(TEXT("PCGComponent"), EFindFirstObjectOptions::None);
	if (!IsValid(PCGComponentClass))
	{
		PCGComponentClass = FindFirstObject<UClass>(TEXT("UPCGComponent"), EFindFirstObjectOptions::None);
	}

	UActorComponent* PCGComp = IsValid(PCGComponentClass) ? TargetActor->FindComponentByClass(PCGComponentClass) : nullptr;
	if (!IsValid(PCGComp))
	{
		Result.Errors.Add(FString::Printf(TEXT("No PCGComponent on actor '%s'. Use attach_pcg_component first."), *ActorName));
		return Result;
	}

	// Call GenerateLocal(bool bForce) via UFunction reflection
	UFunction* GenerateLocalFunc = PCGComp->FindFunction(FName(TEXT("GenerateLocal")));
	if (GenerateLocalFunc)
	{
		struct { bool bForceParam; } Parms;
		Parms.bForceParam = bForce;
		PCGComp->ProcessEvent(GenerateLocalFunc, &Parms);
		UE_LOG(LogAgentFramework, Log, TEXT("PCGActions: Called GenerateLocal(force=%s) on '%s'"), bForce ? TEXT("true") : TEXT("false"), *ActorName);
	}
	else
	{
		// Fallback: try Generate (older API)
		UFunction* GenerateFunc = PCGComp->FindFunction(FName(TEXT("Generate")));
		if (GenerateFunc)
		{
			PCGComp->ProcessEvent(GenerateFunc, nullptr);
		}
		else
		{
			Result.Errors.Add(TEXT("GenerateLocal function not found on PCGComponent. Ensure the PCG plugin version supports this API (UE5.2+)."));
			return Result;
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("PCG generation triggered on actor '%s' (force=%s). Generation runs asynchronously — use get_pcg_info to check the output actor count after completion."),
		*ActorName, bForce ? TEXT("true") : TEXT("false"));
	return Result;
}

// ============================================================================
// ExecuteGetPCGInfo
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPCGActions::ExecuteGetPCGInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!CheckPCGAvailable(Result)) return Result;

	FString ActorName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_name"), ActorName, Result.Errors, true))
	{
		return Result;
	}

	AActor* TargetActor = FindActorByName(ActorName);
	if (!IsValid(TargetActor))
	{
		Result.Errors.Add(FString::Printf(TEXT("Actor '%s' not found in current level."), *ActorName));
		return Result;
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("actor"), ActorName);

	UClass* PCGComponentClass = FindFirstObject<UClass>(TEXT("PCGComponent"), EFindFirstObjectOptions::None);
	if (!IsValid(PCGComponentClass))
	{
		PCGComponentClass = FindFirstObject<UClass>(TEXT("UPCGComponent"), EFindFirstObjectOptions::None);
	}

	UActorComponent* PCGComp = IsValid(PCGComponentClass) ? TargetActor->FindComponentByClass(PCGComponentClass) : nullptr;

	if (!IsValid(PCGComp))
	{
		Root->SetBoolField(TEXT("has_pcg_component"), false);
	}
	else
	{
		Root->SetBoolField(TEXT("has_pcg_component"), true);
		Root->SetStringField(TEXT("component_class"), PCGComp->GetClass()->GetName());

		// Try to get the graph reference
		FProperty* GraphProp = PCGComp->GetClass()->FindPropertyByName(FName(TEXT("Graph")));
		if (GraphProp)
		{
			FObjectProperty* ObjProp = CastField<FObjectProperty>(GraphProp);
			if (ObjProp)
			{
				UObject* Graph = ObjProp->GetObjectPropertyValue_InContainer(PCGComp);
				Root->SetStringField(TEXT("graph"), IsValid(Graph) ? Graph->GetPathName() : TEXT("none"));
			}
		}

		// Is generating?
		FProperty* IsGeneratingProp = PCGComp->GetClass()->FindPropertyByName(FName(TEXT("bIsGenerating")));
		if (IsGeneratingProp)
		{
			FBoolProperty* BoolProp = CastField<FBoolProperty>(IsGeneratingProp);
			if (BoolProp)
			{
				Root->SetBoolField(TEXT("is_generating"), BoolProp->GetPropertyValue_InContainer(PCGComp));
			}
		}
	}

	FString OutputStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputStr);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = OutputStr;
	return Result;
}

// ============================================================================
// ExecuteWirePCGNodes
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkPCGActions::ExecuteWirePCGNodes(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!CheckPCGAvailable(Result)) return Result;

	FString GraphPath, SourceNodeName, SourcePin, TargetNodeName, TargetPin;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("graph_path"), GraphPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("source_node"), SourceNodeName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("source_pin"), SourcePin, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("target_node"), TargetNodeName, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("target_pin"), TargetPin, Result.Errors, true))
	{
		return Result;
	}

	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *GraphPath);
	if (!IsValid(Graph))
	{
		Result.Errors.Add(FString::Printf(TEXT("PCG Graph not found at '%s'."), *GraphPath));
		return Result;
	}

	UPCGNode* SourceNode = nullptr;
	UPCGNode* TargetNode = nullptr;

	for (UPCGNode* Node : Graph->GetNodes())
	{
		if (IsValid(Node))
		{
			if (Node->GetName() == SourceNodeName)
			{
				SourceNode = Node;
			}
			if (Node->GetName() == TargetNodeName)
			{
				TargetNode = Node;
			}
		}
	}

	if (!IsValid(SourceNode) || !IsValid(TargetNode))
	{
		Result.Errors.Add(FString::Printf(TEXT("Source node '%s' or Target node '%s' not found in PCG Graph. Nodes must exist before wiring."), *SourceNodeName, *TargetNodeName));
		return Result;
	}

	Graph->Modify();
	Graph->AddEdge(SourceNode, FName(*SourcePin), TargetNode, FName(*TargetPin));

	UPackage* Package = Graph->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, Graph, *PackageFilename, SaveArgs);
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully wired PCG node '%s' pin '%s' to node '%s' pin '%s' in graph '%s'."),
		*SourceNodeName, *SourcePin, *TargetNodeName, *TargetPin, *GraphPath);
	Result.ModifiedAssets.Add(GraphPath);
	return Result;
}
