// Copyright 2026 AgentFramework. All Rights Reserved.

#include "BehaviorTree/AgentFrameworkBehaviorTreeActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"
#include "Sound/SoundBase.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "UObject/SavePackage.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"

// EQS
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQuery.h"

#define LOCTEXT_NAMESPACE "AgentFrameworkBehaviorTreeActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAgentFrameworkBehaviorTreeActions::FAgentFrameworkBehaviorTreeActions() {}
FAgentFrameworkBehaviorTreeActions::~FAgentFrameworkBehaviorTreeActions() {}

// ============================================================================
// IAgentFrameworkActionExecutor Interface
// ============================================================================

FName FAgentFrameworkBehaviorTreeActions::GetActionName() const { return FName(TEXT("BehaviorTree")); }

TArray<FString> FAgentFrameworkBehaviorTreeActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_blackboard"),
		TEXT("create_behavior_tree"),
		TEXT("inject_bt_nodes"),
		TEXT("configure_navmesh"),
		TEXT("create_state_tree"),
		TEXT("setup_mass_spawner"),
		TEXT("configure_mass_trait"),
		TEXT("setup_mass_crowd"),
		TEXT("query_smart_objects"),
		TEXT("run_eqs")
	};
}

bool FAgentFrameworkBehaviorTreeActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString Action;
	TArray<FString> TempErrors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), Action, TempErrors, false) || Action.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), Action, TempErrors, false);
	}

	if (Action == TEXT("create_blackboard"))
		Result = ExecuteCreateBlackboard(Params, Result);
	else if (Action == TEXT("create_behavior_tree"))
		Result = ExecuteCreateBehaviorTree(Params, Result);
	else if (Action == TEXT("inject_bt_nodes"))
		Result = ExecuteInjectBTNodes(Params, Result);
	else if (Action == TEXT("configure_navmesh"))
		Result = ExecuteConfigureNavMesh(Params, Result);
	else if (Action == TEXT("create_state_tree"))
		Result = ExecuteCreateStateTree(Params, Result);
	else if (Action == TEXT("setup_mass_spawner"))
		Result = ExecuteSetupMassSpawner(Params, Result);
	else if (Action == TEXT("configure_mass_trait"))
		Result = ExecuteConfigureMassTrait(Params, Result);
	else if (Action == TEXT("setup_mass_crowd"))
		Result = ExecuteSetupMassCrowd(Params, Result);
	else if (Action == TEXT("query_smart_objects"))
		Result = ExecuteQuerySmartObjects(Params, Result);
	else if (Action == TEXT("run_eqs"))
		Result = ExecuteRunEQS(Params, Result);
	else
	{
		Result.Errors.Add(TEXT("Unknown BT action."));
	}

	if (Result.bSuccess)
	{
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
// create_blackboard
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteCreateBlackboard(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UBlackboardData::StaticClass(), nullptr);

	if (!NewAsset)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Blackboard at '%s'."), *AssetPath));
		return Result;
	}

	UBlackboardData* Blackboard = Cast<UBlackboardData>(NewAsset);
	if (!IsValid(Blackboard))
	{
		Result.Errors.Add(TEXT("Failed to cast created asset to BlackboardData."));
		return Result;
	}

	// Add keys from JSON array
	const TArray<TSharedPtr<FJsonValue>>* KeysArray = nullptr;
	if (UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("keys"), KeysArray, Result.Errors, false) && KeysArray)
	{
		for (const TSharedPtr<FJsonValue>& KeyVal : *KeysArray)
		{
			if (!KeyVal.IsValid()) continue;
			const TSharedPtr<FJsonObject>* KeyObjPtr = nullptr;
			if (!KeyVal->TryGetObject(KeyObjPtr) || !KeyObjPtr || !KeyObjPtr->IsValid()) continue;
			const TSharedPtr<FJsonObject>& KeyObj = *KeyObjPtr;

			FString KeyName, KeyType;
			UAgentFrameworkActionUtils::TryGetStringParam(KeyObj, TEXT("name"), KeyName, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetStringParam(KeyObj, TEXT("type"), KeyType, Result.Errors, false);

			if (KeyName.IsEmpty() || KeyType.IsEmpty()) continue;

			FBlackboardEntry Entry;
			Entry.EntryName = FName(*KeyName);

			// Map type string to UBlackboardKeyType subclass
			KeyType = KeyType.ToLower();
			if (KeyType == TEXT("bool"))
				Entry.KeyType = NewObject<UBlackboardKeyType_Bool>(Blackboard);
			else if (KeyType == TEXT("int") || KeyType == TEXT("int32"))
				Entry.KeyType = NewObject<UBlackboardKeyType_Int>(Blackboard);
			else if (KeyType == TEXT("float"))
				Entry.KeyType = NewObject<UBlackboardKeyType_Float>(Blackboard);
			else if (KeyType == TEXT("string"))
				Entry.KeyType = NewObject<UBlackboardKeyType_String>(Blackboard);
			else if (KeyType == TEXT("name"))
				Entry.KeyType = NewObject<UBlackboardKeyType_Name>(Blackboard);
			else if (KeyType == TEXT("vector"))
				Entry.KeyType = NewObject<UBlackboardKeyType_Vector>(Blackboard);
			else if (KeyType == TEXT("rotator"))
				Entry.KeyType = NewObject<UBlackboardKeyType_Rotator>(Blackboard);
			else if (KeyType == TEXT("object") || KeyType == TEXT("actor"))
				Entry.KeyType = NewObject<UBlackboardKeyType_Object>(Blackboard);
			else if (KeyType == TEXT("class"))
				Entry.KeyType = NewObject<UBlackboardKeyType_Class>(Blackboard);
			else if (KeyType == TEXT("enum"))
				Entry.KeyType = NewObject<UBlackboardKeyType_Enum>(Blackboard);
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Unknown key type '%s' for key '%s', defaulting to Object."), *KeyType, *KeyName));
				Entry.KeyType = NewObject<UBlackboardKeyType_Object>(Blackboard);
			}

			if (IsValid(Entry.KeyType))
			{
				Blackboard->Keys.Add(Entry);
			}
		}
	}

	Blackboard->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(
		TEXT("Created Blackboard '%s' with %d keys."), *AssetPath, Blackboard->Keys.Num());
	return Result;
}

// ============================================================================
// create_behavior_tree
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteCreateBehaviorTree(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UBehaviorTree::StaticClass(), nullptr);

	if (!NewAsset)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Behavior Tree at '%s'."), *AssetPath));
		return Result;
	}

	UBehaviorTree* BT = Cast<UBehaviorTree>(NewAsset);
	if (!IsValid(BT))
	{
		Result.Errors.Add(TEXT("Failed to cast created asset to Behavior Tree."));
		return Result;
	}

	// Assign blackboard if specified
	FString BlackboardPath;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("blackboard_asset"), BlackboardPath, Result.Errors, false);
	if (!BlackboardPath.IsEmpty())
	{
		UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
		if (IsValid(BB))
		{
			BT->BlackboardAsset = BB;
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Blackboard '%s' not found. BT created without Blackboard assignment."), *BlackboardPath));
		}
	}

	BT->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(
		TEXT("Created Behavior Tree '%s'%s. Use inject_bt_nodes to add Composites, Tasks, Decorators, and Services."),
		*AssetPath,
		BT->BlackboardAsset ? *FString::Printf(TEXT(" with Blackboard '%s'"), *BlackboardPath) : TEXT(""));
	return Result;
}

// ============================================================================
// inject_bt_nodes
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteInjectBTNodes(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *AssetPath);
	if (!IsValid(BT))
	{
		Result.Errors.Add(FString::Printf(TEXT("Behavior Tree not found at '%s'."), *AssetPath));
		return Result;
	}

	// Parse nodes array
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (!UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("nodes"), NodesArray, Result.Errors, true) || !NodesArray)
	{
		return Result;
	}

	int32 NodesAdded = 0;
	FString Report;

	// For now, we support a flat structure: root composite + children
	// More complex hierarchies can be built via multiple inject_bt_nodes calls
	for (const TSharedPtr<FJsonValue>& NodeVal : *NodesArray)
	{
		if (!NodeVal.IsValid()) continue;
		const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
		if (!NodeVal->TryGetObject(NodeObjPtr) || !NodeObjPtr || !NodeObjPtr->IsValid()) continue;
		const TSharedPtr<FJsonObject>& NodeObj = *NodeObjPtr;

		FString NodeType;
		UAgentFrameworkActionUtils::TryGetStringParam(NodeObj, TEXT("type"), NodeType, Result.Errors, false);
		NodeType = NodeType.ToLower();

		FString NodeName;
		UAgentFrameworkActionUtils::TryGetStringParam(NodeObj, TEXT("name"), NodeName, Result.Errors, false);

		if (NodeType == TEXT("selector") || NodeType == TEXT("sequence") || NodeType == TEXT("parallel"))
		{
			// Create composite node
			UBTCompositeNode* Composite = nullptr;

			if (NodeType == TEXT("selector"))
				Composite = NewObject<UBTComposite_Selector>(BT);
			else if (NodeType == TEXT("sequence"))
				Composite = NewObject<UBTComposite_Sequence>(BT);
			else if (NodeType == TEXT("parallel"))
				Composite = NewObject<UBTComposite_SimpleParallel>(BT);

			if (IsValid(Composite))
			{
				Composite->NodeName = NodeName.IsEmpty() ? NodeType : NodeName;

				// If this is the first composite and BT has no root, set as root
				if (!IsValid(BT->RootNode))
				{
					BT->RootNode = Composite;
					Report += FString::Printf(TEXT("Set root node: %s (%s)\n"), *Composite->NodeName, *NodeType);
				}
				else
				{
					// Add as child of root
					FBTCompositeChild Child;
					Child.ChildComposite = Composite;
					BT->RootNode->Children.Add(Child);
					Report += FString::Printf(TEXT("Added child composite: %s (%s)\n"), *Composite->NodeName, *NodeType);
				}
				NodesAdded++;
			}
		}
		else if (NodeType == TEXT("wait"))
		{
			UBTTask_Wait* WaitTask = NewObject<UBTTask_Wait>(BT);
			if (IsValid(WaitTask))
			{
				float WaitTime = 5.0f;
				UAgentFrameworkActionUtils::TryGetFloatParam(NodeObj, TEXT("wait_time"), WaitTime, Result.Errors, false);
				WaitTask->WaitTime = WaitTime;
				WaitTask->NodeName = NodeName.IsEmpty() ? TEXT("Wait") : NodeName;

				if (IsValid(BT->RootNode))
				{
					FBTCompositeChild Child;
					Child.ChildTask = WaitTask;
					BT->RootNode->Children.Add(Child);
					Report += FString::Printf(TEXT("Added Wait task: %.1fs\n"), WaitTime);
					NodesAdded++;
				}
			}
		}
		else if (NodeType == TEXT("moveto") || NodeType == TEXT("move_to"))
		{
			UBTTask_MoveTo* MoveTask = NewObject<UBTTask_MoveTo>(BT);
			if (IsValid(MoveTask))
			{
				MoveTask->NodeName = NodeName.IsEmpty() ? TEXT("MoveTo") : NodeName;
				float AcceptRadius = 50.0f;
				UAgentFrameworkActionUtils::TryGetFloatParam(NodeObj, TEXT("acceptable_radius"), AcceptRadius, Result.Errors, false);
				MoveTask->AcceptableRadius = AcceptRadius;

				if (IsValid(BT->RootNode))
				{
					FBTCompositeChild Child;
					Child.ChildTask = MoveTask;
					BT->RootNode->Children.Add(Child);
					Report += FString::Printf(TEXT("Added MoveTo task (radius: %.0f)\n"), AcceptRadius);
					NodesAdded++;
				}
			}
		}
		else
		{
			Report += FString::Printf(TEXT("Skipped unknown node type: '%s'\n"), *NodeType);
		}
	}

	BT->MarkPackageDirty();

	Result.bSuccess = NodesAdded > 0;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(
		TEXT("Injected %d nodes into BT '%s':\n%s"),
		NodesAdded, *AssetPath, *Report);

	if (NodesAdded == 0)
	{
		Result.Errors.Add(TEXT("No nodes were added. Check node type names. Supported: selector, sequence, parallel, wait, moveto"));
	}

	return Result;
}

// ============================================================================
// configure_navmesh
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteConfigureNavMesh(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("No editor world available."));
		return Result;
	}

	// Check if NavMeshBoundsVolume already exists
	bool bVolumeExists = false;
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		bVolumeExists = true;
		break;
	}

	FString Report;

	if (!bVolumeExists)
	{
		// Parse bounds (default: large volume covering most gameplay areas)
		FVector Location(0.0f, 0.0f, 0.0f);
		FVector Scale(50.0f, 50.0f, 10.0f);  // Each unit = 200 UU, so 50 = 10000 UU = ~100m

		const TSharedPtr<FJsonObject>* LocationObj = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("location"), LocationObj, Result.Errors, false) && LocationObj && LocationObj->IsValid())
		{
			UAgentFrameworkActionUtils::TryGetDoubleParam(*LocationObj, TEXT("x"), Location.X, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*LocationObj, TEXT("y"), Location.Y, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*LocationObj, TEXT("z"), Location.Z, Result.Errors, false);
		}

		const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("scale"), ScaleObj, Result.Errors, false) && ScaleObj && ScaleObj->IsValid())
		{
			UAgentFrameworkActionUtils::TryGetDoubleParam(*ScaleObj, TEXT("x"), Scale.X, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*ScaleObj, TEXT("y"), Scale.Y, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*ScaleObj, TEXT("z"), Scale.Z, Result.Errors, false);
		}

		// Spawn the NavMeshBoundsVolume
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ANavMeshBoundsVolume* NavVolume = World->SpawnActor<ANavMeshBoundsVolume>(
			ANavMeshBoundsVolume::StaticClass(),
			FTransform(FRotator::ZeroRotator, Location, Scale),
			SpawnParams);

		if (IsValid(NavVolume))
		{
			Report += FString::Printf(TEXT("Spawned NavMeshBoundsVolume at (%s) with scale (%s)\n"),
				*Location.ToString(), *Scale.ToString());
		}
		else
		{
			Result.Errors.Add(TEXT("Failed to spawn NavMeshBoundsVolume."));
			return Result;
		}
	}
	else
	{
		Report += TEXT("NavMeshBoundsVolume already exists in the level.\n");
	}

	// Trigger NavMesh rebuild
	bool bRebuild = true;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("rebuild"), bRebuild, Result.Errors, false);

	if (bRebuild)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (IsValid(NavSys))
		{
			NavSys->Build();
			Report += TEXT("NavMesh rebuild triggered.\n");
		}
		else
		{
			Report += TEXT("WARNING: NavigationSystemV1 not found. Ensure NavigationSystem is enabled in Project Settings.\n");
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = Report;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteCreateStateTree(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UClass* StateTreeClass = FindFirstObject<UClass>(TEXT("StateTree"), EFindFirstObjectOptions::None);
	if (!IsValid(StateTreeClass))
	{
		StateTreeClass = FindFirstObject<UClass>(TEXT("UStateTree"), EFindFirstObjectOptions::None);
	}

	if (!IsValid(StateTreeClass))
	{
		Result.Errors.Add(TEXT("StateTreeModule is not loaded in this project. Add 'StateTreeModule' to your host project's .Build.cs."));
		return Result;
	}

	FString PackageName, PackagePath, AssetName;
	UAgentFrameworkActionUtils::SplitAssetPath(AssetPath, PackageName, PackagePath, AssetName);

	if (AssetName.IsEmpty() || PackagePath.IsEmpty())
	{
		Result.Errors.Add(FString::Printf(
			TEXT("asset_path '%s' does not name an asset. Provide a full path including the asset name, e.g. /Game/AI/ST_Guard."),
			*AssetPath));
		return Result;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* StateTree = AssetTools.CreateAsset(AssetName, PackagePath, StateTreeClass, nullptr);

	if (!IsValid(StateTree))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create State Tree at '%s'."), *AssetPath));
		return Result;
	}

	UPackage* Package = StateTree->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, StateTree, *PackageFilename, SaveArgs);
		}
	}
	FAssetRegistryModule::AssetCreated(StateTree);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully created StateTree '%s'."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteSetupMassSpawner(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("No active editor world found."));
		return Result;
	}

	UClass* SpawnerClass = FindFirstObject<UClass>(TEXT("MassSpawner"), EFindFirstObjectOptions::None);
	if (!IsValid(SpawnerClass))
	{
		Result.Errors.Add(TEXT("MassSpawner module is not loaded in this project. Add 'MassSpawner' to your host project's .Build.cs."));
		return Result;
	}

	AActor* Spawner = World->SpawnActor<AActor>(SpawnerClass);
	if (!IsValid(Spawner))
	{
		Result.Errors.Add(TEXT("Failed to spawn MassSpawner actor."));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Spawned Mass Spawner '%s' in editor world."), *Spawner->GetName());
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteConfigureMassTrait(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClass* TraitClass = FindFirstObject<UClass>(TEXT("MassEntityTraitBase"), EFindFirstObjectOptions::None);
	if (!IsValid(TraitClass))
	{
		Result.Errors.Add(TEXT("MassEntity module is not loaded in this project. Add 'MassEntity' to your host project's .Build.cs."));
		return Result;
	}
	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully located and verified Mass Entity Trait class: %s"), *TraitClass->GetName());
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteSetupMassCrowd(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClass* CrowdSubsystemClass = FindFirstObject<UClass>(TEXT("MassCrowdSubsystem"), EFindFirstObjectOptions::None);
	if (!IsValid(CrowdSubsystemClass))
	{
		Result.Errors.Add(TEXT("MassCrowd module is not loaded in this project. Add 'MassCrowd' to your host project's .Build.cs."));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("Verified MassCrowdSubsystem class availability via reflection.");
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteQuerySmartObjects(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClass* SOSubsystemClass = FindFirstObject<UClass>(TEXT("SmartObjectSubsystem"), EFindFirstObjectOptions::None);
	if (!IsValid(SOSubsystemClass))
	{
		Result.Errors.Add(TEXT("SmartObjectsModule is not loaded in this project. Add 'SmartObjectsModule' to your host project's .Build.cs."));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("Verified SmartObjectSubsystem class availability via reflection.");
	return Result;
}


FAgentFrameworkActionResult FAgentFrameworkBehaviorTreeActions::ExecuteRunEQS(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString QueryTemplatePath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("query_template_path"), QueryTemplatePath, Result.Errors, true))
	{
		return Result;
	}

	UEnvQuery* QueryTemplate = LoadObject<UEnvQuery>(nullptr, *QueryTemplatePath);
	if (!IsValid(QueryTemplate))
	{
		Result.Errors.Add(FString::Printf(TEXT("EQS Query template not found at '%s'."), *QueryTemplatePath));
		return Result;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("No active editor world found."));
		return Result;
	}

	FEnvQueryRequest QueryRequest(QueryTemplate, World);
	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully initialized FEnvQueryRequest using query template '%s'."), *QueryTemplatePath);
	return Result;
}

#undef LOCTEXT_NAMESPACE
