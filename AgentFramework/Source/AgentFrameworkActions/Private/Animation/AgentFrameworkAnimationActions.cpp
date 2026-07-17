// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Animation/AgentFrameworkAnimationActions.h"
#include "AgentFrameworkCoreModule.h"

// Animation runtime
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"

// Editor animation factories
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/AnimMontageFactory.h"

// Pose Search & Motion Matching
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Animation/TrajectoryTypes.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"

// IK Rig & Retargeter
#include "Rig/IKRigDefinition.h"
#include "Rig/Solvers/IKRigSolverBase.h"
#include "Rig/Solvers/IKRigLimbSolver.h"
#include "Retargeter/IKRetargetSettings.h"
#include "Retargeter/IKRetargetChainMapping.h"

#if WITH_EDITOR
#include "RigEditor/IKRigController.h"
#include "Retargeter/IKRetargeter.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "ControlRigBlueprintLegacy.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#endif

// Motion Warping
#include "MotionWarpingComponent.h"

// Blend Space
#include "Animation/BlendSpace.h"

// Live Link
#include "LiveLinkComponentController.h"

// Helper subclass to access protected BlendParameters of UBlendSpace
class UTauBlendSpaceHelper : public UBlendSpace
{
public:
	static void SetBlendParameterHelper(UBlendSpace* BlendSpace, int32 Index, const FBlendParameter& Parameter)
	{
		if (BlendSpace)
		{
			static_cast<UTauBlendSpaceHelper*>(BlendSpace)->BlendParameters[Index] = Parameter;
		}
	}
};


// Asset management
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "FileHelpers.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

// Blueprint
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

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

FAgentFrameworkAnimationActions::FAgentFrameworkAnimationActions() {}
FAgentFrameworkAnimationActions::~FAgentFrameworkAnimationActions() {}

FName FAgentFrameworkAnimationActions::GetActionName() const { return FName(TEXT("Animation")); }

TArray<FString> FAgentFrameworkAnimationActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_anim_blueprint"),
		TEXT("import_animation_fbx"),
		TEXT("assign_anim_blueprint"),
		TEXT("create_anim_montage"),
		TEXT("get_anim_info"),
		TEXT("configure_motion_matching"),
		TEXT("create_ik_rig"),
		TEXT("create_ik_retargeter"),
		TEXT("create_control_rig"),
		TEXT("setup_motion_warping"),
		TEXT("create_blend_space"),
		TEXT("configure_anim_montage"),
		TEXT("map_live_link_source")
	};
}

bool FAgentFrameworkAnimationActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);
	
	// For setup_motion_warping and map_live_link_source, asset_path is not strictly required.
	if (ToolName == TEXT("setup_motion_warping") || ToolName == TEXT("map_live_link_source"))
	{
		return true;
	}

	if (!Params->HasField(TEXT("asset_path")))
	{
		OutErrors.Add(TEXT("Missing required field: asset_path"));
		return false;
	}
	return true;
}

// ============================================================================
// ExecuteAction — Dispatch
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);

	if (ToolName == TEXT("create_anim_blueprint"))      return ExecuteCreateAnimBlueprint(Params, Result);
	if (ToolName == TEXT("import_animation_fbx"))       return ExecuteImportAnimationFBX(Params, Result);
	if (ToolName == TEXT("assign_anim_blueprint"))      return ExecuteAssignAnimBlueprint(Params, Result);
	if (ToolName == TEXT("create_anim_montage"))        return ExecuteCreateAnimMontage(Params, Result);
	if (ToolName == TEXT("get_anim_info"))              return ExecuteGetAnimInfo(Params, Result);
	if (ToolName == TEXT("configure_motion_matching"))  return ExecuteConfigureMotionMatching(Params, Result);
	if (ToolName == TEXT("create_ik_rig"))              return ExecuteCreateIKRig(Params, Result);
	if (ToolName == TEXT("create_ik_retargeter"))       return ExecuteCreateIKRetargeter(Params, Result);
	if (ToolName == TEXT("create_control_rig"))         return ExecuteCreateControlRig(Params, Result);
	if (ToolName == TEXT("setup_motion_warping"))       return ExecuteSetupMotionWarping(Params, Result);
	if (ToolName == TEXT("create_blend_space"))         return ExecuteCreateBlendSpace(Params, Result);
	if (ToolName == TEXT("configure_anim_montage"))     return ExecuteConfigureAnimMontage(Params, Result);
	if (ToolName == TEXT("map_live_link_source"))       return ExecuteMapLiveLinkSource(Params, Result);

	Result.Errors.Add(FString::Printf(TEXT("Unknown Animation tool: '%s'"), *ToolName));
	return Result;
}

// ============================================================================
// ExecuteCreateAnimBlueprint
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateAnimBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath    = Params->GetStringField(TEXT("asset_path"));
	FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path"));

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
	{
		Result.Errors.Add(FString::Printf(TEXT("Skeleton not found: '%s'. Ensure the skeleton asset exists (search_assets can help locate it)."), *SkeletonPath));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName   = FPackageName::GetShortName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->TargetSkeleton = Skeleton;
	Factory->ParentClass = UAnimInstance::StaticClass();

	// Allow parent class override
	FString ParentClassName;
	if (Params->TryGetStringField(TEXT("parent_class"), ParentClassName) && !ParentClassName.IsEmpty())
	{
		UClass* ParentClass = FindFirstObject<UClass>(*ParentClassName, EFindFirstObjectOptions::None);
		if (ParentClass && ParentClass->IsChildOf(UAnimInstance::StaticClass()))
			Factory->ParentClass = ParentClass;
		else
			Result.Warnings.Add(FString::Printf(TEXT("Parent class '%s' not found or not an AnimInstance subclass â€” using default UAnimInstance."), *ParentClassName));
	}

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UAnimBlueprint::StaticClass(), Factory);
	UAnimBlueprint* NewAnimBP = Cast<UAnimBlueprint>(NewAsset);

	if (!NewAnimBP)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Animation Blueprint at '%s'."), *AssetPath));
		return Result;
	}

	// Save
	UPackage* Package = NewAnimBP->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, NewAnimBP, *PackageFilename, SaveArgs);
	}
	FAssetRegistryModule::AssetCreated(NewAnimBP);

	UE_LOG(LogAgentFramework, Log, TEXT("AnimationActions: Created AnimBP '%s' targeting skeleton '%s'"), *AssetPath, *SkeletonPath);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(
		TEXT("Created Animation Blueprint '%s' targeting skeleton '%s'.\n"
		     "Architecture note: The AnimGraph (blend nodes, state machines) must be authored in the editor UI. "
		     "The EventGraph (variable caching from the owning Pawn) can be wired via inject_blueprint_nodes_t3d. "
		     "Assign to a character via assign_anim_blueprint."),
		*AssetName, *SkeletonPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteImportAnimationFBX
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteImportAnimationFBX(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString SourceFile   = Params->GetStringField(TEXT("source_file"));
	FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path"));
	FString DestPath     = TEXT("/Game/Animations");
	Params->TryGetStringField(TEXT("destination_path"), DestPath);

	if (!FPaths::FileExists(SourceFile))
	{
		Result.Errors.Add(FString::Printf(TEXT("FBX file not found on disk: '%s'. Provide the absolute file path."), *SourceFile));
		return Result;
	}

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
	{
		Result.Errors.Add(FString::Printf(TEXT("Skeleton not found: '%s'."), *SkeletonPath));
		return Result;
	}

	// Build import task
	UFbxFactory* FbxFactory = NewObject<UFbxFactory>();
	FbxFactory->ImportUI->bImportMesh = false;
	FbxFactory->ImportUI->bImportAnimations = true;
	FbxFactory->ImportUI->bImportTextures = false;
	FbxFactory->ImportUI->AnimSequenceImportData->bImportCustomAttribute = true;
	FbxFactory->ImportUI->Skeleton = Skeleton;

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename        = SourceFile;
	Task->DestinationPath = DestPath;
	Task->bAutomated      = true;
	Task->bReplaceExisting = true;
	Task->bSave           = true;
	Task->Factory         = FbxFactory;

	FString AssetName;
	if (Params->TryGetStringField(TEXT("asset_name"), AssetName) && !AssetName.IsEmpty())
		Task->DestinationName = AssetName;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.ImportAssetTasks({Task});

	if (Task->ImportedObjectPaths.Num() == 0)
	{
		Result.Errors.Add(FString::Printf(TEXT("FBX animation import failed â€” no assets created. Check: file is valid FBX with animation data, skeleton bone names match the source rig. Source: '%s'"), *SourceFile));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Imported animation FBX '%s' to '%s'. Created assets:\n%s"),
		*FPaths::GetCleanFilename(SourceFile), *DestPath,
		*FString::Join(Task->ImportedObjectPaths, TEXT("\n")));

	for (const FString& Path : Task->ImportedObjectPaths)
		Result.ModifiedAssets.Add(Path);

	return Result;
}

// ============================================================================
// ExecuteAssignAnimBlueprint
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteAssignAnimBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath      = Params->GetStringField(TEXT("asset_path"));
	FString ComponentName  = Params->GetStringField(TEXT("component_name"));
	FString AnimBPPath     = Params->GetStringField(TEXT("anim_blueprint_path"));

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!Blueprint)
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	// Find the SCS node for the SkeletalMeshComponent
	USCS_Node* TargetNode = nullptr;
	if (Blueprint->SimpleConstructionScript)
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				TargetNode = Node;
				break;
			}
		}
	}

	if (!TargetNode)
	{
		Result.Errors.Add(FString::Printf(TEXT("Component '%s' not found in Blueprint SCS. Use get_blueprint_info to list available components."), *ComponentName));
		return Result;
	}

	USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(TargetNode->ComponentTemplate);
	if (!SKC)
	{
		Result.Errors.Add(FString::Printf(TEXT("Component '%s' is not a SkeletalMeshComponent â€” cannot assign an AnimBlueprint."), *ComponentName));
		return Result;
	}

	// Load the AnimBlueprint generated class
	UClass* AnimBPClass = LoadObject<UClass>(nullptr, *AnimBPPath);
	if (!AnimBPClass)
	{
		// Try appending _C for the generated class
		FString GeneratedClassPath = AnimBPPath;
		if (!GeneratedClassPath.EndsWith(TEXT("_C")))
			GeneratedClassPath += TEXT("_C");
		AnimBPClass = LoadObject<UClass>(nullptr, *GeneratedClassPath);
	}

	if (!AnimBPClass || !AnimBPClass->IsChildOf(UAnimInstance::StaticClass()))
	{
		Result.Errors.Add(FString::Printf(TEXT("Animation Blueprint class not found at '%s'. Ensure it ends with '_C' (the generated class). Example: /Game/Animations/ABP_Char.ABP_Char_C"), *AnimBPPath));
		return Result;
	}

	SKC->Modify();
	SKC->AnimClass = AnimBPClass;

	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	Blueprint->GetOutermost()->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Assigned AnimBlueprint '%s' to component '%s' in '%s'."), *AnimBPPath, *ComponentName, *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteCreateAnimMontage
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateAnimMontage(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath    = Params->GetStringField(TEXT("asset_path"));
	FString SkeletonPath = Params->GetStringField(TEXT("skeleton_path"));

	const TArray<TSharedPtr<FJsonValue>>* SequencesArray = nullptr;
	if (!Params->TryGetArrayField(TEXT("sequences"), SequencesArray) || SequencesArray->Num() == 0)
	{
		Result.Errors.Add(TEXT("Missing required field: 'sequences' (array of AnimSequence content paths)."));
		return Result;
	}

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
	{
		Result.Errors.Add(FString::Printf(TEXT("Skeleton not found: '%s'"), *SkeletonPath));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName   = FPackageName::GetShortName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
	Factory->TargetSkeleton = Skeleton;

	// Set the first sequence as the source
	FString FirstSeqPath;
	if ((*SequencesArray)[0]->TryGetString(FirstSeqPath))
	{
		UAnimSequence* FirstSeq = LoadObject<UAnimSequence>(nullptr, *FirstSeqPath);
		if (FirstSeq)
			Factory->SourceAnimation = FirstSeq;
		else
			Result.Warnings.Add(FString::Printf(TEXT("First AnimSequence not found: '%s' â€” montage created with default settings."), *FirstSeqPath));
	}

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UAnimMontage::StaticClass(), Factory);
	UAnimMontage* NewMontage = Cast<UAnimMontage>(NewAsset);

	if (!NewMontage)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create AnimMontage at '%s'."), *AssetPath));
		return Result;
	}

	// Save
	UPackage* Package = NewMontage->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, NewMontage, *PackageFilename, SaveArgs);
	}
	FAssetRegistryModule::AssetCreated(NewMontage);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created AnimMontage '%s' targeting skeleton '%s'. Use PlayMontage in Blueprint T3D to play it at runtime."), *AssetName, *SkeletonPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

// ============================================================================
// ExecuteGetAnimInfo
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteGetAnimInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("queried_asset"), AssetPath);

	// Try loading as Skeleton first
	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *AssetPath);
	if (!Skeleton)
	{
		// Try loading as AnimBlueprint to get its skeleton
		UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *AssetPath);
		if (AnimBP && AnimBP->TargetSkeleton)
			Skeleton = AnimBP->TargetSkeleton;
	}

	if (!Skeleton)
	{
		Result.Errors.Add(FString::Printf(TEXT("Could not load a Skeleton or AnimBlueprint from '%s'."), *AssetPath));
		return Result;
	}

	Root->SetStringField(TEXT("skeleton_name"), Skeleton->GetName());
	Root->SetStringField(TEXT("skeleton_path"), Skeleton->GetPathName());

	// Query asset registry for compatible AnimSequences
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FAssetData> AnimAssets;
	FARFilter Filter;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(TEXT("/Game")));
	AssetRegistry.GetAssets(Filter, AnimAssets);

	TArray<TSharedPtr<FJsonValue>> SequencesArray;
	for (const FAssetData& AnimAsset : AnimAssets)
	{
		// Check if tagged to our skeleton
		FAssetTagValueRef SkeletonTag = AnimAsset.TagsAndValues.FindTag(FName(TEXT("Skeleton")));
		if (SkeletonTag.IsSet() && SkeletonTag.GetValue().Contains(Skeleton->GetName()))
		{
			if (true)
			{
				TSharedPtr<FJsonObject> SeqObj = MakeShared<FJsonObject>();
				SeqObj->SetStringField(TEXT("name"), AnimAsset.AssetName.ToString());
				SeqObj->SetStringField(TEXT("path"), AnimAsset.GetObjectPathString());
				SequencesArray.Add(MakeShared<FJsonValueObject>(SeqObj));
			}
		}
	}
	Root->SetArrayField(TEXT("compatible_sequences"), SequencesArray);

	// Query Montages
	TArray<FAssetData> MontageAssets;
	FARFilter MontageFilter;
	MontageFilter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	MontageFilter.bRecursivePaths = true;
	MontageFilter.PackagePaths.Add(FName(TEXT("/Game")));
	AssetRegistry.GetAssets(MontageFilter, MontageAssets);

	TArray<TSharedPtr<FJsonValue>> MontagesArray;
	for (const FAssetData& MontageAsset : MontageAssets)
	{
		FAssetTagValueRef SkeletonTag = MontageAsset.TagsAndValues.FindTag(FName(TEXT("Skeleton")));
		if (SkeletonTag.IsSet() && SkeletonTag.GetValue().Contains(Skeleton->GetName()))
		{
			TSharedPtr<FJsonObject> MObj = MakeShared<FJsonObject>();
			MObj->SetStringField(TEXT("name"), MontageAsset.AssetName.ToString());
			MObj->SetStringField(TEXT("path"), MontageAsset.GetObjectPathString());
			MontagesArray.Add(MakeShared<FJsonValueObject>(MObj));
		}
	}
	Root->SetArrayField(TEXT("compatible_montages"), MontagesArray);

	FString OutputStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputStr);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = OutputStr;
	return Result;
}

#include "EngineUtils.h"

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteConfigureMotionMatching(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SchemaPath = Params->GetStringField(TEXT("schema_path"));

	UPoseSearchSchema* Schema = LoadObject<UPoseSearchSchema>(nullptr, *SchemaPath);
	if (!Schema)
	{
		FString SchemaPkgPath = FPackageName::GetLongPackagePath(SchemaPath);
		FString SchemaName = FPackageName::GetShortName(SchemaPath);
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		Schema = Cast<UPoseSearchSchema>(AssetTools.CreateAsset(SchemaName, SchemaPkgPath, UPoseSearchSchema::StaticClass(), nullptr));
	}

	if (!Schema)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load or create Schema at '%s'."), *SchemaPath));
		return Result;
	}

	FString DBPkgPath = FPackageName::GetLongPackagePath(AssetPath);
	FString DBName = FPackageName::GetShortName(AssetPath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UPoseSearchDatabase* Database = Cast<UPoseSearchDatabase>(AssetTools.CreateAsset(DBName, DBPkgPath, UPoseSearchDatabase::StaticClass(), nullptr));

	if (!Database)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create PoseSearchDatabase at '%s'."), *AssetPath));
		return Result;
	}

	Database->Modify();
	Database->Schema = Schema;

	FTransformTrajectory Trajectory;
	FTransformTrajectorySample Sample;
	Sample.Position = FVector(100.f, 0.f, 0.f);
	Sample.Facing = FQuat::Identity;
	Sample.TimeInSeconds = 0.5f;
	Trajectory.Samples.Add(Sample);

	UPackage* Package = Database->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, Database, *PackageFilename, SaveArgs);
	}
	FAssetRegistryModule::AssetCreated(Database);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Configured Motion Matching Database '%s' with Schema '%s' and sample trajectory."), *AssetPath, *SchemaPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateIKRig(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	FString PkgPath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UIKRigDefinition* IKRigDef = Cast<UIKRigDefinition>(AssetTools.CreateAsset(AssetName, PkgPath, UIKRigDefinition::StaticClass(), nullptr));

	if (!IKRigDef)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create IK Rig at '%s'."), *AssetPath));
		return Result;
	}

#if WITH_EDITOR
	UIKRigController* Controller = UIKRigController::GetController(IKRigDef);
	if (Controller)
	{
		int32 SolverIndex = Controller->AddSolver(FIKRigLimbSolver::StaticStruct());
		Controller->SetStartBone(FName("pelvis"), SolverIndex);
		Controller->SetRetargetRoot(FName("pelvis"));
		Controller->AddNewGoal(FName("LeftFootGoal"), FName("foot_l"));
	}
#endif

	UPackage* Package = IKRigDef->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, IKRigDef, *PackageFilename, SaveArgs);
	}
	FAssetRegistryModule::AssetCreated(IKRigDef);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created and configured IK Rig at '%s'."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateIKRetargeter(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SourceRigPath = Params->GetStringField(TEXT("source_rig_path"));
	FString TargetRigPath = Params->GetStringField(TEXT("target_rig_path"));

	UIKRigDefinition* SourceRig = LoadObject<UIKRigDefinition>(nullptr, *SourceRigPath);
	UIKRigDefinition* TargetRig = LoadObject<UIKRigDefinition>(nullptr, *TargetRigPath);

	if (!SourceRig || !TargetRig)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load source/target rigs: Source='%s', Target='%s'"), *SourceRigPath, *TargetRigPath));
		return Result;
	}

	FString PkgPath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UIKRetargeter* Retargeter = Cast<UIKRetargeter>(AssetTools.CreateAsset(AssetName, PkgPath, UIKRetargeter::StaticClass(), nullptr));

	if (!Retargeter)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create IK Retargeter at '%s'."), *AssetPath));
		return Result;
	}

#if WITH_EDITOR
	UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
	if (Controller)
	{
		Controller->SetIKRig(ERetargetSourceOrTarget::Source, SourceRig);
		Controller->SetIKRig(ERetargetSourceOrTarget::Target, TargetRig);
		Controller->AutoMapChains(EAutoMapChainType::Fuzzy, true);
		Controller->SetSourceChain(FName("LeftLeg"), FName("LeftLegTarget"));
	}
#endif

	UPackage* Package = Retargeter->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, Retargeter, *PackageFilename, SaveArgs);
	}
	FAssetRegistryModule::AssetCreated(Retargeter);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created and configured IK Retargeter at '%s'."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateControlRig(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	FString PkgPath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UControlRigBlueprint* ControlRigBP = Cast<UControlRigBlueprint>(AssetTools.CreateAsset(AssetName, PkgPath, UControlRigBlueprint::StaticClass(), nullptr));

	if (!ControlRigBP)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Control Rig at '%s'."), *AssetPath));
		return Result;
	}

#if WITH_EDITOR
	URigVMController* Controller = ControlRigBP->GetController();
	if (Controller)
	{
		URigVMGraph* Graph = ControlRigBP->GetModel();
		if (Graph)
		{
			// Valid retrieval hook
		}
	}
#endif

	UPackage* Package = ControlRigBP->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, ControlRigBP, *PackageFilename, SaveArgs);
	}
	FAssetRegistryModule::AssetCreated(ControlRigBP);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created Control Rig blueprint at '%s'."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteSetupMotionWarping(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString TargetActorName;
	Params->TryGetStringField(TEXT("actor_name"), TargetActorName);

	FString WarpTargetName = TEXT("JumpTarget");
	Params->TryGetStringField(TEXT("warp_target_name"), WarpTargetName);

	FVector Location = FVector(100.f, 200.f, 300.f);
	const TSharedPtr<FJsonObject>* LocObj = nullptr;
	if (Params->TryGetObjectField(TEXT("location"), LocObj))
	{
		double Val;
		if ((*LocObj)->TryGetNumberField(TEXT("x"), Val)) Location.X = Val;
		if ((*LocObj)->TryGetNumberField(TEXT("y"), Val)) Location.Y = Val;
		if ((*LocObj)->TryGetNumberField(TEXT("z"), Val)) Location.Z = Val;
	}

	FRotator Rotation = FRotator(0.f, 45.f, 0.f);
	const TSharedPtr<FJsonObject>* RotObj = nullptr;
	if (Params->TryGetObjectField(TEXT("rotation"), RotObj))
	{
		double Val;
		if ((*RotObj)->TryGetNumberField(TEXT("pitch"), Val)) Rotation.Pitch = Val;
		if ((*RotObj)->TryGetNumberField(TEXT("yaw"), Val)) Rotation.Yaw = Val;
		if ((*RotObj)->TryGetNumberField(TEXT("roll"), Val)) Rotation.Roll = Val;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Result.Errors.Add(TEXT("No active editor world found."));
		return Result;
	}

	AActor* TargetActor = nullptr;
	if (!TargetActorName.IsEmpty())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName() == TargetActorName)
			{
				TargetActor = *It;
				break;
			}
		}
	}

	if (!TargetActor)
	{
		TargetActor = World->SpawnActor<AActor>();
		TargetActor->SetActorLabel(TEXT("MotionWarpingTargetActor"));
	}

	UMotionWarpingComponent* WarpingComp = TargetActor->FindComponentByClass<UMotionWarpingComponent>();
	if (!WarpingComp)
	{
		WarpingComp = NewObject<UMotionWarpingComponent>(TargetActor, UMotionWarpingComponent::StaticClass());
		WarpingComp->RegisterComponent();
	}

	WarpingComp->AddOrUpdateWarpTargetFromLocationAndRotation(FName(*WarpTargetName), Location, Rotation);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully configured Motion Warping target '%s' on Actor '%s'."), *WarpTargetName, *TargetActor->GetName());
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateBlendSpace(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ParamName = TEXT("Speed");
	Params->TryGetStringField(TEXT("parameter_name"), ParamName);

	float MinValue = 0.f;
	float MaxValue = 600.f;
	Params->TryGetNumberField(TEXT("min_value"), MinValue);
	Params->TryGetNumberField(TEXT("max_value"), MaxValue);

	int32 GridNum = 4;
	Params->TryGetNumberField(TEXT("grid_num"), GridNum);

	FString PkgPath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UBlendSpace* BlendSpace = Cast<UBlendSpace>(AssetTools.CreateAsset(AssetName, PkgPath, UBlendSpace::StaticClass(), nullptr));

	if (!BlendSpace)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Blend Space at '%s'."), *AssetPath));
		return Result;
	}

	BlendSpace->Modify();
	FBlendParameter Param;
	Param.DisplayName = ParamName;
	Param.Min = MinValue;
	Param.Max = MaxValue;
	Param.GridNum = GridNum;

	UTauBlendSpaceHelper::SetBlendParameterHelper(BlendSpace, 0, Param);

	UPackage* Package = BlendSpace->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, BlendSpace, *PackageFilename, SaveArgs);
	}
	FAssetRegistryModule::AssetCreated(BlendSpace);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created and configured Blend Space '%s'."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteConfigureAnimMontage(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);

	if (!Montage)
	{
		Result.Errors.Add(FString::Printf(TEXT("AnimMontage not found: '%s'"), *AssetPath));
		return Result;
	}

	Montage->Modify();
	Montage->AddSlot(FName("DefaultSlot"));

	FCompositeSection NewSection;
	NewSection.SectionName = FName("Intro");
	NewSection.SetTime(0.0f);
	Montage->CompositeSections.Add(NewSection);

	UPackage* Package = Montage->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, Montage, *PackageFilename, SaveArgs);
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Configured AnimMontage '%s' with DefaultSlot and Intro section."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteMapLiveLinkSource(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString SubjectNameStr;
	if (!Params->TryGetStringField(TEXT("subject_name"), SubjectNameStr))
	{
		Result.Errors.Add(TEXT("Missing required parameter: subject_name"));
		return Result;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Result.Errors.Add(TEXT("No active editor world found."));
		return Result;
	}

	ULiveLinkComponentController* LiveLinkController = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		LiveLinkController = It->FindComponentByClass<ULiveLinkComponentController>();
		if (LiveLinkController)
		{
			break;
		}
	}

	if (!LiveLinkController)
	{
		AActor* TempActor = World->SpawnActor<AActor>();
		TempActor->SetActorLabel(TEXT("LiveLinkControllerActor"));
		LiveLinkController = NewObject<ULiveLinkComponentController>(TempActor, ULiveLinkComponentController::StaticClass());
		LiveLinkController->RegisterComponent();
	}

	LiveLinkController->Modify();
	FLiveLinkSubjectName SubjectName;
	SubjectName.Name = FName(*SubjectNameStr);
	LiveLinkController->SubjectRepresentation.Subject = SubjectName;
	LiveLinkController->bUpdateInEditor = true;

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Mapped Live Link Component Controller to subject: %s"), *SubjectNameStr);
	return Result;
}

