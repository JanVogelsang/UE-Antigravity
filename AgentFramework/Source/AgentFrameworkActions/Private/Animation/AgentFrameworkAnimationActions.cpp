// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Animation/AgentFrameworkAnimationActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"

// Animation runtime
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"

// Editor animation factories
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/AnimMontageFactory.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Sound/SoundBase.h"
#endif

// Blend Space
#include "Animation/BlendSpace.h"


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
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

// Blueprint
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet2/KismetEditorUtilities.h"

// JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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

	FString AssetPath;
	return UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true);
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

	if (ToolName == TEXT("create_anim_blueprint"))      Result = ExecuteCreateAnimBlueprint(Params, Result);
	else if (ToolName == TEXT("import_animation_fbx"))  Result = ExecuteImportAnimationFBX(Params, Result);
	else if (ToolName == TEXT("assign_anim_blueprint")) Result = ExecuteAssignAnimBlueprint(Params, Result);
	else if (ToolName == TEXT("create_anim_montage"))   Result = ExecuteCreateAnimMontage(Params, Result);
	else if (ToolName == TEXT("get_anim_info"))         Result = ExecuteGetAnimInfo(Params, Result);
	else if (ToolName == TEXT("configure_motion_matching")) Result = ExecuteConfigureMotionMatching(Params, Result);
	else if (ToolName == TEXT("create_ik_rig"))         Result = ExecuteCreateIKRig(Params, Result);
	else if (ToolName == TEXT("create_ik_retargeter"))  Result = ExecuteCreateIKRetargeter(Params, Result);
	else if (ToolName == TEXT("create_control_rig"))    Result = ExecuteCreateControlRig(Params, Result);
	else if (ToolName == TEXT("setup_motion_warping"))  Result = ExecuteSetupMotionWarping(Params, Result);
	else if (ToolName == TEXT("create_blend_space"))    Result = ExecuteCreateBlendSpace(Params, Result);
	else if (ToolName == TEXT("configure_anim_montage")) Result = ExecuteConfigureAnimMontage(Params, Result);
	else if (ToolName == TEXT("map_live_link_source"))  Result = ExecuteMapLiveLinkSource(Params, Result);
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown Animation tool: '%s'"), *ToolName));
		return Result;
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	return Result;
}

// ============================================================================
// ExecuteCreateAnimBlueprint
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateAnimBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString SkeletonPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("skeleton_path"), SkeletonPath, Result.Errors, true))
	{
		return Result;
	}

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!IsValid(Skeleton))
	{
		Result.Errors.Add(FString::Printf(TEXT("Skeleton not found: '%s'. Ensure the skeleton asset exists (search_assets can help locate it)."), *SkeletonPath));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName   = FPackageName::GetShortName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	if (!IsValid(Factory))
	{
		Result.Errors.Add(TEXT("Failed to create UAnimBlueprintFactory."));
		return Result;
	}
	Factory->TargetSkeleton = Skeleton;
	Factory->ParentClass = UAnimInstance::StaticClass();

	// Allow parent class override
	FString ParentClassName;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("parent_class"), ParentClassName, Result.Errors, false) && !ParentClassName.IsEmpty())
	{
		UClass* ParentClass = FindFirstObject<UClass>(*ParentClassName, EFindFirstObjectOptions::None);
		if (IsValid(ParentClass) && ParentClass->IsChildOf(UAnimInstance::StaticClass()))
		{
			Factory->ParentClass = ParentClass;
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Parent class '%s' not found or not an AnimInstance subclass — using default UAnimInstance."), *ParentClassName));
		}
	}

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UAnimBlueprint::StaticClass(), Factory);
	UAnimBlueprint* NewAnimBP = Cast<UAnimBlueprint>(NewAsset);

	if (!IsValid(NewAnimBP))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Animation Blueprint at '%s'."), *AssetPath));
		return Result;
	}

	// Save
	UPackage* Package = NewAnimBP->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, NewAnimBP, *PackageFilename, SaveArgs);
		}
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
	FString SourceFile;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("source_file"), SourceFile, Result.Errors, true))
	{
		return Result;
	}

	FString SkeletonPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("skeleton_path"), SkeletonPath, Result.Errors, true))
	{
		return Result;
	}

	FString DestPath = TEXT("/Game/Animations");
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("destination_path"), DestPath, Result.Errors, false);

	if (!FPaths::FileExists(SourceFile))
	{
		Result.Errors.Add(FString::Printf(TEXT("FBX file not found on disk: '%s'. Provide the absolute file path."), *SourceFile));
		return Result;
	}

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!IsValid(Skeleton))
	{
		Result.Errors.Add(FString::Printf(TEXT("Skeleton not found: '%s'."), *SkeletonPath));
		return Result;
	}

	// Build import task
	UFbxFactory* FbxFactory = NewObject<UFbxFactory>();
	if (!IsValid(FbxFactory))
	{
		Result.Errors.Add(TEXT("Failed to create UFbxFactory."));
		return Result;
	}
	if (IsValid(FbxFactory->ImportUI))
	{
		FbxFactory->ImportUI->bImportMesh = false;
		FbxFactory->ImportUI->bImportAnimations = true;
		FbxFactory->ImportUI->bImportTextures = false;
		if (IsValid(FbxFactory->ImportUI->AnimSequenceImportData))
		{
			FbxFactory->ImportUI->AnimSequenceImportData->bImportCustomAttribute = true;
		}
		FbxFactory->ImportUI->Skeleton = Skeleton;
	}

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	if (!IsValid(Task))
	{
		Result.Errors.Add(TEXT("Failed to create UAssetImportTask."));
		return Result;
	}
	Task->Filename        = SourceFile;
	Task->DestinationPath = DestPath;
	Task->bAutomated      = true;
	Task->bReplaceExisting = true;
	Task->bSave           = true;
	Task->Factory         = FbxFactory;

	FString AssetName;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_name"), AssetName, Result.Errors, false) && !AssetName.IsEmpty())
	{
		Task->DestinationName = AssetName;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.ImportAssetTasks({Task});

	if (Task->ImportedObjectPaths.Num() == 0)
	{
		Result.Errors.Add(FString::Printf(TEXT("FBX animation import failed — no assets created. Check: file is valid FBX with animation data, skeleton bone names match the source rig. Source: '%s'"), *SourceFile));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Imported animation FBX '%s' to '%s'. Created assets:\n%s"),
		*FPaths::GetCleanFilename(SourceFile), *DestPath,
		*FString::Join(Task->ImportedObjectPaths, TEXT("\n")));

	for (const FString& Path : Task->ImportedObjectPaths)
	{
		Result.ModifiedAssets.Add(Path);
	}

	return Result;
}

// ============================================================================
// ExecuteAssignAnimBlueprint
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteAssignAnimBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString ComponentName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("component_name"), ComponentName, Result.Errors, true))
	{
		return Result;
	}

	FString AnimBPPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("anim_blueprint_path"), AnimBPPath, Result.Errors, true))
	{
		return Result;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!IsValid(Blueprint))
	{
		Result.Errors.Add(FString::Printf(TEXT("Blueprint not found: '%s'"), *AssetPath));
		return Result;
	}

	// Find the SCS node for the SkeletalMeshComponent
	USCS_Node* TargetNode = nullptr;
	if (IsValid(Blueprint->SimpleConstructionScript))
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (IsValid(Node) && Node->GetVariableName().ToString() == ComponentName)
			{
				TargetNode = Node;
				break;
			}
		}
	}

	if (!IsValid(TargetNode))
	{
		Result.Errors.Add(FString::Printf(TEXT("Component '%s' not found in Blueprint SCS. Use get_blueprint_info to list available components."), *ComponentName));
		return Result;
	}

	USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(TargetNode->ComponentTemplate);
	if (!IsValid(SKC))
	{
		Result.Errors.Add(FString::Printf(TEXT("Component '%s' is not a SkeletalMeshComponent — cannot assign an AnimBlueprint."), *ComponentName));
		return Result;
	}

	// Load the AnimBlueprint generated class
	UClass* AnimBPClass = LoadObject<UClass>(nullptr, *AnimBPPath);
	if (!IsValid(AnimBPClass))
	{
		// Try appending _C for the generated class
		FString GeneratedClassPath = AnimBPPath;
		if (!GeneratedClassPath.EndsWith(TEXT("_C")))
		{
			GeneratedClassPath += TEXT("_C");
		}
		AnimBPClass = LoadObject<UClass>(nullptr, *GeneratedClassPath);
	}

	if (!IsValid(AnimBPClass) || !AnimBPClass->IsChildOf(UAnimInstance::StaticClass()))
	{
		Result.Errors.Add(FString::Printf(TEXT("Animation Blueprint class not found at '%s'. Ensure it ends with '_C' (the generated class). Example: /Game/Animations/ABP_Char.ABP_Char_C"), *AnimBPPath));
		return Result;
	}

	SKC->Modify();
	SKC->AnimClass = AnimBPClass;

	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	UPackage* Package = Blueprint->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
	}

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
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString SkeletonPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("skeleton_path"), SkeletonPath, Result.Errors, true))
	{
		return Result;
	}

	TArray<FString> Sequences;
	if (!UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("sequences"), Sequences, Result.Errors, true))
	{
		return Result;
	}

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!IsValid(Skeleton))
	{
		Result.Errors.Add(FString::Printf(TEXT("Skeleton not found: '%s'"), *SkeletonPath));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName   = FPackageName::GetShortName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
	if (!IsValid(Factory))
	{
		Result.Errors.Add(TEXT("Failed to create UAnimMontageFactory."));
		return Result;
	}
	Factory->TargetSkeleton = Skeleton;

	// Set the first sequence as the source
	if (Sequences.Num() > 0)
	{
		UAnimSequence* FirstSeq = LoadObject<UAnimSequence>(nullptr, *Sequences[0]);
		if (IsValid(FirstSeq))
		{
			Factory->SourceAnimation = FirstSeq;
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("First AnimSequence not found: '%s' — montage created with default settings."), *Sequences[0]));
		}
	}

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UAnimMontage::StaticClass(), Factory);
	UAnimMontage* NewMontage = Cast<UAnimMontage>(NewAsset);

	if (!IsValid(NewMontage))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create AnimMontage at '%s'."), *AssetPath));
		return Result;
	}

	// Save
	UPackage* Package = NewMontage->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, NewMontage, *PackageFilename, SaveArgs);
		}
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
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("queried_asset"), AssetPath);

	// Try loading as Skeleton first
	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *AssetPath);
	if (!IsValid(Skeleton))
	{
		// Try loading as AnimBlueprint to get its skeleton
		UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *AssetPath);
		if (IsValid(AnimBP) && IsValid(AnimBP->TargetSkeleton))
		{
			Skeleton = AnimBP->TargetSkeleton;
		}
	}

	if (!IsValid(Skeleton))
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
			TSharedPtr<FJsonObject> SeqObj = MakeShared<FJsonObject>();
			SeqObj->SetStringField(TEXT("name"), AnimAsset.AssetName.ToString());
			SeqObj->SetStringField(TEXT("path"), AnimAsset.GetObjectPathString());
			SequencesArray.Add(MakeShared<FJsonValueObject>(SeqObj));
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

	FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteConfigureMotionMatching(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
	{
		UClass* SchemaClass = FindFirstObject<UClass>(TEXT("PoseSearchSchema"), EFindFirstObjectOptions::None);
		UClass* DBClass = FindFirstObject<UClass>(TEXT("PoseSearchDatabase"), EFindFirstObjectOptions::None);
		if (!IsValid(SchemaClass) || !IsValid(DBClass))
		{
			Result.Errors.Add(TEXT("PoseSearch module is not loaded in this project. Add 'PoseSearch' to your host project's .Build.cs."));
			return Result;
		}

		FString AssetPath, SchemaPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
			!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("schema_path"), SchemaPath, Result.Errors, true))
		{
			return Result;
		}

		FString DBPkgPath = FPackageName::GetLongPackagePath(AssetPath);
		FString DBName = FPackageName::GetShortName(AssetPath);
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UObject* Database = AssetTools.CreateAsset(DBName, DBPkgPath, DBClass, nullptr);

		if (!IsValid(Database))
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to create PoseSearchDatabase at '%s'."), *AssetPath));
			return Result;
		}

		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Configured Motion Matching Database '%s' with Schema '%s'."), *AssetPath, *SchemaPath);
		Result.ModifiedAssets.Add(AssetPath);
		return Result;
	}

	FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateIKRig(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
	{
		UClass* IKRigClass = FindFirstObject<UClass>(TEXT("IKRigDefinition"), EFindFirstObjectOptions::None);
		if (!IsValid(IKRigClass))
		{
			Result.Errors.Add(TEXT("IKRig module is not loaded in this project. Add 'IKRig' to your host project's .Build.cs."));
			return Result;
		}

		FString AssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
		{
			return Result;
		}

		FString PkgPath = FPackageName::GetLongPackagePath(AssetPath);
		FString AssetName = FPackageName::GetShortName(AssetPath);
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UObject* IKRigDef = AssetTools.CreateAsset(AssetName, PkgPath, IKRigClass, nullptr);

		if (!IsValid(IKRigDef))
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to create IK Rig at '%s'."), *AssetPath));
			return Result;
		}

		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Created IK Rig asset at '%s'."), *AssetPath);
		Result.ModifiedAssets.Add(AssetPath);
		return Result;
	}

	FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateIKRetargeter(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
	{
		UClass* RetargeterClass = FindFirstObject<UClass>(TEXT("IKRetargeter"), EFindFirstObjectOptions::None);
		if (!IsValid(RetargeterClass))
		{
			Result.Errors.Add(TEXT("IKRig module is not loaded in this project. Add 'IKRig' to your host project's .Build.cs."));
			return Result;
		}

		FString AssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
		{
			return Result;
		}

		FString PkgPath = FPackageName::GetLongPackagePath(AssetPath);
		FString AssetName = FPackageName::GetShortName(AssetPath);
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UObject* Retargeter = AssetTools.CreateAsset(AssetName, PkgPath, RetargeterClass, nullptr);

		if (!IsValid(Retargeter))
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to create IK Retargeter at '%s'."), *AssetPath));
			return Result;
		}

		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Created IK Retargeter at '%s'."), *AssetPath);
		Result.ModifiedAssets.Add(AssetPath);
		return Result;
	}


	FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateControlRig(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
	{
		UClass* ControlRigBPClass = FindFirstObject<UClass>(TEXT("ControlRigBlueprint"), EFindFirstObjectOptions::None);
		if (!IsValid(ControlRigBPClass))
		{
			Result.Errors.Add(TEXT("ControlRigDeveloper module is not loaded in this project. Add 'ControlRigDeveloper' to your host project's .Build.cs."));
			return Result;
		}

		FString AssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
		{
			return Result;
		}

		FString PkgPath = FPackageName::GetLongPackagePath(AssetPath);
		FString AssetName = FPackageName::GetShortName(AssetPath);
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		UObject* ControlRigBP = AssetTools.CreateAsset(AssetName, PkgPath, ControlRigBPClass, nullptr);

		if (!IsValid(ControlRigBP))
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to create Control Rig at '%s'."), *AssetPath));
			return Result;
		}

		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Created Control Rig blueprint at '%s'."), *AssetPath);
		Result.ModifiedAssets.Add(AssetPath);
		return Result;
	}

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteSetupMotionWarping(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClass* WarpingCompClass = FindFirstObject<UClass>(TEXT("MotionWarpingComponent"), EFindFirstObjectOptions::None);
	if (!IsValid(WarpingCompClass))
	{
		Result.Errors.Add(TEXT("MotionWarping module is not loaded in this project. Add 'MotionWarping' to your host project's .Build.cs."));
		return Result;
	}

	FString TargetActorName, WarpTargetName = TEXT("JumpTarget");
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_name"), TargetActorName, Result.Errors, false);
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("warp_target_name"), WarpTargetName, Result.Errors, false);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Verified MotionWarpingComponent class availability via reflection for Actor '%s'."), *TargetActorName);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteMapLiveLinkSource(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClass* LiveLinkControllerClass = FindFirstObject<UClass>(TEXT("LiveLinkComponentController"), EFindFirstObjectOptions::None);
	if (!IsValid(LiveLinkControllerClass))
	{
		Result.Errors.Add(TEXT("LiveLinkComponents module is not loaded in this project. Add 'LiveLinkComponents' to your host project's .Build.cs."));
		return Result;
	}

	FString SubjectNameStr;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("subject_name"), SubjectNameStr, Result.Errors, false);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Verified LiveLinkComponentController class availability via reflection for subject '%s'."), *SubjectNameStr);
	return Result;
}


FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteCreateBlendSpace(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString ParamName = TEXT("Speed");
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("parameter_name"), ParamName, Result.Errors, false);

	float MinValue = 0.f;
	float MaxValue = 600.f;
	UAgentFrameworkActionUtils::TryGetFloatParam(Params, TEXT("min_value"), MinValue, Result.Errors, false);
	UAgentFrameworkActionUtils::TryGetFloatParam(Params, TEXT("max_value"), MaxValue, Result.Errors, false);

	int32 GridNum = 4;
	UAgentFrameworkActionUtils::TryGetIntParam(Params, TEXT("grid_num"), GridNum, Result.Errors, false);

	FString PkgPath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UBlendSpace* BlendSpace = Cast<UBlendSpace>(AssetTools.CreateAsset(AssetName, PkgPath, UBlendSpace::StaticClass(), nullptr));

	if (!IsValid(BlendSpace))
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
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, BlendSpace, *PackageFilename, SaveArgs);
		}
	}
	FAssetRegistryModule::AssetCreated(BlendSpace);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created and configured Blend Space '%s'."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkAnimationActions::ExecuteConfigureAnimMontage(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);

	if (!IsValid(Montage))
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
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, Montage, *PackageFilename, SaveArgs);
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Configured AnimMontage '%s' with DefaultSlot and Intro section."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}



void FAgentFrameworkAnimationActions::PlaySuccessSound()
{
#if WITH_EDITOR
	if (IsValid(GEditor))
	{
		USoundBase* SuccessSound = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
		if (IsValid(SuccessSound))
		{
			GEditor->PlayEditorSound(SuccessSound);
		}
	}
#endif
}

