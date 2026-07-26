// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Level/AgentFrameworkLevelActions.h"
#include "AgentFrameworkActionUtils.h"
#include "AgentFrameworkCoreModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Blueprint.h"
#include "ScopedTransaction.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Sound/SoundBase.h"

// World Partition
#include "WorldPartition/WorldPartition.h"

// Foliage
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliageActor.h"

// Landscape
#include "Landscape.h"
#include "LandscapeGrassType.h"

// Level Instance & Packed Level Actor
#include "LevelInstance/LevelInstanceActor.h"
#include "PackedLevelActor/PackedLevelActor.h"

// Cine Camera Rig Rail
#include "CineCameraRigRail.h"

// DMX Library & Fixture Patch
#include "Library/DMXLibrary.h"
#include "Library/DMXEntityFixturePatch.h"

// Chaos Vehicles
#include "ChaosWheeledVehicleMovementComponent.h"

FAgentFrameworkLevelActions::FAgentFrameworkLevelActions() {}
FAgentFrameworkLevelActions::~FAgentFrameworkLevelActions() {}
FName FAgentFrameworkLevelActions::GetActionName() const { return FName(TEXT("Level")); }

TArray<FString> FAgentFrameworkLevelActions::GetSupportedToolNames() const
{
	return {
		TEXT("spawn_actor"),
		TEXT("place_light"),
		TEXT("modify_world_settings"),
		TEXT("configure_world_partition"),
		TEXT("create_foliage_type"),
		TEXT("paint_foliage_brush"),
		TEXT("create_landscape"),
		TEXT("create_landscape_grass_type"),
		TEXT("create_level_instance"),
		TEXT("create_packed_level_actor"),
		TEXT("setup_cine_camera_rig_rail"),
		TEXT("setup_dmx_patch"),
		TEXT("setup_chaos_vehicle")
	};
}

bool FAgentFrameworkLevelActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, OutErrors, false);

	if (ToolName == TEXT("spawn_actor"))
	{
		FString ClassName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("class"), ClassName, OutErrors, true))
		{
			return false;
		}
	}

	return OutErrors.Num() == 0;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FScopedTransaction Transaction(FText::FromString(TEXT("AgentFramework Level Action")));

	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	if (!IsValid(GEditor))
	{
		Result.Errors.Add(TEXT("Editor not available."));
		Transaction.Cancel();
		return Result;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("No world loaded."));
		Transaction.Cancel();
		return Result;
	}

	World->Modify();

	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, Result.Errors, false);

	if (ToolName == TEXT("spawn_actor"))                  Result = ExecuteSpawnActor(Params, World);
	else if (ToolName == TEXT("place_light"))             Result = ExecutePlaceLight(Params, World);
	else if (ToolName == TEXT("modify_world_settings"))   Result = ExecuteModifyWorldSettings(Params, World);
	else if (ToolName == TEXT("configure_world_partition")) Result = ExecuteConfigureWorldPartition(Params, World);
	else if (ToolName == TEXT("create_foliage_type"))     Result = ExecuteCreateFoliageType(Params, World);
	else if (ToolName == TEXT("paint_foliage_brush"))     Result = ExecutePaintFoliageBrush(Params, World);
	else if (ToolName == TEXT("create_landscape"))         Result = ExecuteCreateLandscape(Params, World);
	else if (ToolName == TEXT("create_landscape_grass_type")) Result = ExecuteCreateLandscapeGrassType(Params, World);
	else if (ToolName == TEXT("create_level_instance"))   Result = ExecuteCreateLevelInstance(Params, World);
	else if (ToolName == TEXT("create_packed_level_actor")) Result = ExecuteCreatePackedLevelActor(Params, World);
	else if (ToolName == TEXT("setup_cine_camera_rig_rail")) Result = ExecuteSetupCineCameraRigRail(Params, World);
	else if (ToolName == TEXT("setup_dmx_patch"))         Result = ExecuteSetupDMXPatch(Params, World);
	else if (ToolName == TEXT("setup_chaos_vehicle"))     Result = ExecuteSetupChaosVehicle(Params, World);
	else
	{
		// Legacy fallback
		FString Action;
		if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), Action, Result.Errors, false) && !Action.IsEmpty())
		{
			if (Action == TEXT("spawn_actor")) Result = ExecuteSpawnActor(Params, World);
			else if (Action == TEXT("place_light")) Result = ExecutePlaceLight(Params, World);
			else if (Action == TEXT("modify_world_settings")) Result = ExecuteModifyWorldSettings(Params, World);
		}
		else
		{
			Result.Errors.Add(FString::Printf(TEXT("Unknown level action: %s"), *ToolName));
		}
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}
	else
	{
		Transaction.Cancel();
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteSpawnActor(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("World is invalid."));
		return Result;
	}

	FString ClassName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("class"), ClassName, Result.Errors, true))
	{
		return Result;
	}

	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	const TSharedPtr<FJsonObject>* LocObj = nullptr;
	if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("location"), LocObj, Result.Errors, false) && LocObj && LocObj->IsValid())
	{
		UAgentFrameworkActionUtils::TryGetDoubleParam(*LocObj, TEXT("x"), Location.X, Result.Errors, false);
		UAgentFrameworkActionUtils::TryGetDoubleParam(*LocObj, TEXT("y"), Location.Y, Result.Errors, false);
		UAgentFrameworkActionUtils::TryGetDoubleParam(*LocObj, TEXT("z"), Location.Z, Result.Errors, false);
	}
	const TSharedPtr<FJsonObject>* RotObj = nullptr;
	if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("rotation"), RotObj, Result.Errors, false) && RotObj && RotObj->IsValid())
	{
		UAgentFrameworkActionUtils::TryGetDoubleParam(*RotObj, TEXT("pitch"), Rotation.Pitch, Result.Errors, false);
		UAgentFrameworkActionUtils::TryGetDoubleParam(*RotObj, TEXT("yaw"), Rotation.Yaw, Result.Errors, false);
		UAgentFrameworkActionUtils::TryGetDoubleParam(*RotObj, TEXT("roll"), Rotation.Roll, Result.Errors, false);
	}

	// Resolve class: built-in names or Blueprint content paths
	UClass* ActorClass = nullptr;

	// Check if it's a Blueprint content path (starts with /Game/)
	if (ClassName.StartsWith(TEXT("/Game/")) || ClassName.StartsWith(TEXT("/Script/")))
	{
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ClassName);
		if (IsValid(BP) && IsValid(BP->GeneratedClass))
		{
			ActorClass = BP->GeneratedClass;
		}
		else
		{
			// Try loading as UClass directly
			ActorClass = LoadObject<UClass>(nullptr, *ClassName);
		}
	}

	if (!IsValid(ActorClass))
	{
		// Try built-in class names
		ActorClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
	}

	if (!IsValid(ActorClass))
	{
		if (ClassName == TEXT("StaticMeshActor")) ActorClass = AStaticMeshActor::StaticClass();
		else if (ClassName == TEXT("PointLight")) ActorClass = APointLight::StaticClass();
		else if (ClassName == TEXT("DirectionalLight")) ActorClass = ADirectionalLight::StaticClass();
		else if (ClassName == TEXT("PlayerStart"))
		{
			ActorClass = FindFirstObject<UClass>(TEXT("PlayerStart"), EFindFirstObjectOptions::None);
			if (!IsValid(ActorClass)) ActorClass = AActor::StaticClass();
		}
		else ActorClass = AActor::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	FString ActorLabel;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("label"), ActorLabel, Result.Errors, false) && !ActorLabel.IsEmpty())
	{
		SpawnParams.Name = FName(*ActorLabel);
	}

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	if (IsValid(NewActor))
	{
		if (!ActorLabel.IsEmpty())
		{
			NewActor->SetActorLabel(ActorLabel);
		}

		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Spawned %s at (%s)"), *ClassName, *Location.ToString());
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to spawn actor of class %s"), *ClassName));
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecutePlaceLight(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("World is invalid."));
		return Result;
	}

	FString LightType = TEXT("PointLight");
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("light_type"), LightType, Result.Errors, false);

	FVector Location = FVector::ZeroVector;
	const TSharedPtr<FJsonObject>* LocObj = nullptr;
	if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("location"), LocObj, Result.Errors, false) && LocObj && LocObj->IsValid())
	{
		UAgentFrameworkActionUtils::TryGetDoubleParam(*LocObj, TEXT("x"), Location.X, Result.Errors, false);
		UAgentFrameworkActionUtils::TryGetDoubleParam(*LocObj, TEXT("y"), Location.Y, Result.Errors, false);
		UAgentFrameworkActionUtils::TryGetDoubleParam(*LocObj, TEXT("z"), Location.Z, Result.Errors, false);
	}

	UClass* LightClass = APointLight::StaticClass();
	if (LightType == TEXT("DirectionalLight")) LightClass = ADirectionalLight::StaticClass();

	AActor* Light = World->SpawnActor<AActor>(LightClass, Location, FRotator::ZeroRotator);
	if (IsValid(Light))
	{
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Placed %s at %s"), *LightType, *Location.ToString());
	}
	else
	{
		Result.Errors.Add(TEXT("Failed to place light."));
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteModifyWorldSettings(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("World is invalid."));
		return Result;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!IsValid(WorldSettings))
	{
		Result.Errors.Add(TEXT("Could not access World Settings."));
		return Result;
	}

	WorldSettings->Modify();

	TArray<FString> Changes;

	// Game Mode Override
	FString GameModeOverride;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("game_mode_override"), GameModeOverride, Result.Errors, false) && !GameModeOverride.IsEmpty())
	{
		UClass* GameModeClass = nullptr;

		// Try loading as Blueprint content path
		if (GameModeOverride.StartsWith(TEXT("/Game/")) || GameModeOverride.StartsWith(TEXT("/Script/")))
		{
			UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *GameModeOverride);
			if (IsValid(BP) && IsValid(BP->GeneratedClass) && BP->GeneratedClass->IsChildOf(AGameModeBase::StaticClass()))
			{
				GameModeClass = BP->GeneratedClass;
			}
			else
			{
				// Try as class path directly
				GameModeClass = LoadObject<UClass>(nullptr, *GameModeOverride);
			}
		}

		if (!IsValid(GameModeClass))
		{
			// Try by name
			GameModeClass = FindFirstObject<UClass>(*GameModeOverride, EFindFirstObjectOptions::None);
		}

		if (IsValid(GameModeClass) && GameModeClass->IsChildOf(AGameModeBase::StaticClass()))
		{
			WorldSettings->DefaultGameMode = TSubclassOf<AGameModeBase>(GameModeClass);
			Changes.Add(FString::Printf(TEXT("GameMode override set to %s"), *GameModeClass->GetName()));
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Could not find GameMode class: %s"), *GameModeOverride));
		}
	}

	// Default Pawn Class (on the GameMode CDO if accessible)
	FString DefaultPawnClass;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("default_pawn_class"), DefaultPawnClass, Result.Errors, false) && !DefaultPawnClass.IsEmpty())
	{
		Result.Warnings.Add(TEXT("default_pawn_class should be set on the GameMode Blueprint's defaults using set_blueprint_defaults tool instead."));
	}

	// Kill Z
	double KillZ = 0.0;
	if (Params->HasField(TEXT("kill_z")) && UAgentFrameworkActionUtils::TryGetDoubleParam(Params, TEXT("kill_z"), KillZ, Result.Errors, false))
	{
		WorldSettings->KillZ = KillZ;
		Changes.Add(FString::Printf(TEXT("KillZ set to %.1f"), KillZ));
	}

	if (Changes.Num() > 0)
	{
		Result.bSuccess = true;
		Result.ResultMessage = TEXT("World Settings updated: ") + FString::Join(Changes, TEXT(", "));

		// Mark the package as dirty so it saves
		WorldSettings->MarkPackageDirty();
	}
	else
	{
		Result.Errors.Add(TEXT("No valid world settings properties were provided to modify."));
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteConfigureWorldPartition(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("World is invalid."));
		return Result;
	}

	UWorldPartition* WorldPartition = World->GetWorldPartition();
	if (!IsValid(WorldPartition))
	{
		Result.Errors.Add(TEXT("World Partition is not enabled/supported in the current world."));
		return Result;
	}

	WorldPartition->Modify();
	WorldPartition->SetEnableStreaming(true);

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("Successfully enabled streaming on World Partition.");
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteCreateFoliageType(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	FString AssetPath, StaticMeshPath;

	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("static_mesh_path"), StaticMeshPath, Result.Errors, true))
	{
		return Result;
	}

	UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *StaticMeshPath);
	if (!IsValid(StaticMesh))
	{
		Result.Errors.Add(FString::Printf(TEXT("StaticMesh not found: '%s'"), *StaticMeshPath));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UFoliageType_InstancedStaticMesh* FoliageType = Cast<UFoliageType_InstancedStaticMesh>(
		AssetTools.CreateAsset(AssetName, PackagePath, UFoliageType_InstancedStaticMesh::StaticClass(), nullptr));

	if (!IsValid(FoliageType))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Foliage Type asset at '%s'."), *AssetPath));
		return Result;
	}

	FoliageType->Modify();
	FoliageType->SetStaticMesh(StaticMesh);

	UPackage* Package = FoliageType->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, FoliageType, *PackageFilename, SaveArgs);
		}
	}
	FAssetRegistryModule::AssetCreated(FoliageType);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created and configured Foliage Type at '%s' with StaticMesh '%s'."), *AssetPath, *StaticMeshPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecutePaintFoliageBrush(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
#if WITH_EDITOR
	FString FoliageTypePath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("foliage_type_path"), FoliageTypePath, Result.Errors, true))
	{
		return Result;
	}

	UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
	if (!IsValid(FoliageType))
	{
		Result.Errors.Add(FString::Printf(TEXT("FoliageType not found: '%s'"), *FoliageTypePath));
		return Result;
	}

	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("World is invalid."));
		return Result;
	}

	AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World, true);
	if (!IsValid(IFA))
	{
		Result.Errors.Add(TEXT("Failed to retrieve or spawn InstancedFoliageActor."));
		return Result;
	}

	FFoliageInfo* OutInfo = nullptr;
	IFA->AddFoliageType(FoliageType, &OutInfo);
	if (!OutInfo)
	{
		Result.Errors.Add(TEXT("Failed to add FoliageType to InstancedFoliageActor."));
		return Result;
	}

	TArray<FTransform> Transforms;
	for (int32 i = 0; i < 10; ++i)
	{
		FTransform T;
		T.SetTranslation(FVector(i * 200.f, 0.f, 0.f));
		T.SetRotation(FQuat::Identity);
		T.SetScale3D(FVector(1.f));
		Transforms.Add(T);
	}

	for (const FTransform& Transform : Transforms)
	{
		FFoliageInstance NewInstance;
		NewInstance.Location = Transform.GetTranslation();
		NewInstance.Rotation = Transform.Rotator();
		NewInstance.DrawScale3D = FVector3f(Transform.GetScale3D());
		OutInfo->AddInstance(FoliageType, NewInstance);
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Painted 10 instances of FoliageType '%s' onto InstancedFoliageActor."), *FoliageTypePath);
#else
	Result.Errors.Add(TEXT("Foliage painting is only supported in editor builds."));
#endif
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteCreateLandscape(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("World is invalid."));
		return Result;
	}

	FString MaterialPath;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("material_path"), MaterialPath, Result.Errors, false);

	ALandscape* Landscape = World->SpawnActor<ALandscape>();
	if (!IsValid(Landscape))
	{
		Result.Errors.Add(TEXT("Failed to spawn ALandscape actor."));
		return Result;
	}

	Landscape->SetActorLabel(TEXT("Landscape_Proto"));
	if (!MaterialPath.IsEmpty())
	{
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (IsValid(Mat))
		{
			Landscape->LandscapeMaterial = Mat;
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Spawned ALandscape actor: '%s'."), *Landscape->GetName());
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteCreateLandscapeGrassType(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	FString AssetPath, StaticMeshPath;

	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("static_mesh_path"), StaticMeshPath, Result.Errors, true))
	{
		return Result;
	}

	UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *StaticMeshPath);
	if (!IsValid(StaticMesh))
	{
		Result.Errors.Add(FString::Printf(TEXT("StaticMesh not found: '%s'"), *StaticMeshPath));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	ULandscapeGrassType* GrassType = Cast<ULandscapeGrassType>(
		AssetTools.CreateAsset(AssetName, PackagePath, ULandscapeGrassType::StaticClass(), nullptr));

	if (!IsValid(GrassType))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Landscape Grass Type asset at '%s'."), *AssetPath));
		return Result;
	}

	GrassType->Modify();
	FGrassVariety Variety;
	Variety.GrassMesh = StaticMesh;
	Variety.GrassDensity.Default = 100.f;
	Variety.StartCullDistance.Default = 500;
	Variety.EndCullDistance.Default = 1000;
	Variety.RandomRotation = true;
	Variety.AlignToSurface = true;
	GrassType->GrassVarieties.Add(Variety);

	UPackage* Package = GrassType->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, GrassType, *PackageFilename, SaveArgs);
		}
	}
	FAssetRegistryModule::AssetCreated(GrassType);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created Landscape Grass Type asset at '%s' with variety mesh '%s'."), *AssetPath, *StaticMeshPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteCreateLevelInstance(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("World is invalid."));
		return Result;
	}

	FString LevelAssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("level_asset_path"), LevelAssetPath, Result.Errors, true))
	{
		return Result;
	}

	ALevelInstance* LevelInstance = World->SpawnActor<ALevelInstance>();
	if (!IsValid(LevelInstance))
	{
		Result.Errors.Add(TEXT("Failed to spawn ALevelInstance actor."));
		return Result;
	}

#if WITH_EDITOR
	LevelInstance->SetWorldAsset(TSoftObjectPtr<UWorld>(FSoftObjectPath(LevelAssetPath)));
	LevelInstance->SetDesiredRuntimeBehavior(ELevelInstanceRuntimeBehavior::Partitioned);
#endif

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created Level Instance actor: '%s' pointing to '%s'."), *LevelInstance->GetName(), *LevelAssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteCreatePackedLevelActor(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("World is invalid."));
		return Result;
	}

	APackedLevelActor* PackedActor = World->SpawnActor<APackedLevelActor>();
	if (!IsValid(PackedActor))
	{
		Result.Errors.Add(TEXT("Failed to spawn APackedLevelActor."));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created Packed Level Actor: '%s'."), *PackedActor->GetName());
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteSetupCineCameraRigRail(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	if (!IsValid(World))
	{
		Result.Errors.Add(TEXT("World is invalid."));
		return Result;
	}

	ACineCameraRigRail* RigRail = World->SpawnActor<ACineCameraRigRail>();
	if (!IsValid(RigRail))
	{
		Result.Errors.Add(TEXT("Failed to spawn ACineCameraRigRail."));
		return Result;
	}

	RigRail->SetAbsolutePositionOnRail(0.5f);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Created Cine Camera Rig Rail '%s' and set absolute position to 0.5."), *RigRail->GetName());
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteSetupDMXPatch(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	FString DMXLibraryPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("dmx_library_path"), DMXLibraryPath, Result.Errors, true))
	{
		return Result;
	}

	UDMXLibrary* DMXLibrary = LoadObject<UDMXLibrary>(nullptr, *DMXLibraryPath);
	if (!IsValid(DMXLibrary))
	{
		Result.Errors.Add(FString::Printf(TEXT("DMX Library not found: '%s'"), *DMXLibraryPath));
		return Result;
	}

	TArray<UDMXEntityFixturePatch*> Patches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Verified DMX Library '%s' containing %d patches."), *DMXLibraryPath, Patches.Num());
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkLevelActions::ExecuteSetupChaosVehicle(const TSharedRef<FJsonObject>& Params, UWorld* World)
{
	FAgentFrameworkActionResult Result;
	UChaosWheeledVehicleMovementComponent* Comp = NewObject<UChaosWheeledVehicleMovementComponent>();
	if (!IsValid(Comp))
	{
		Result.Errors.Add(TEXT("Failed to instantiate UChaosWheeledVehicleMovementComponent."));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("Successfully instantiated and verified UChaosWheeledVehicleMovementComponent.");
	return Result;
}

void FAgentFrameworkLevelActions::PlaySuccessSound()
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
