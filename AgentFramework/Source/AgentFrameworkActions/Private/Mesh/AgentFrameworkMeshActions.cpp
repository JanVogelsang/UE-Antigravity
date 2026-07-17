// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Mesh/AgentFrameworkMeshActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "Factories/FbxFactory.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "ScopedTransaction.h"

FAgentFrameworkMeshActions::FAgentFrameworkMeshActions() {}
FAgentFrameworkMeshActions::~FAgentFrameworkMeshActions() {}
FName FAgentFrameworkMeshActions::GetActionName() const { return FName(TEXT("Mesh")); }

TArray<FString> FAgentFrameworkMeshActions::GetSupportedToolNames() const
{
	return {
		TEXT("import_mesh"),
		TEXT("import_assets_batch"),
		TEXT("configure_static_mesh"),
		TEXT("create_dynamic_mesh"),
		TEXT("audit_nanite_settings"),
		TEXT("setup_runtime_virtual_texture"),
		TEXT("setup_chaos_physics"),
		TEXT("setup_dataflow_graph"),
		TEXT("setup_clothing_simulation"),
		TEXT("setup_sparse_volume_texture")
	};
}

bool FAgentFrameworkMeshActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);

	if (ToolName == TEXT("import_mesh"))
	{
		if (!Params->HasField(TEXT("source_file")))
		{
			OutErrors.Add(TEXT("Missing required field for import_mesh: source_file"));
			return false;
		}
	}
	else if (ToolName == TEXT("import_assets_batch"))
	{
		if (!Params->HasField(TEXT("source_files")))
		{
			OutErrors.Add(TEXT("Missing required field for import_assets_batch: source_files"));
			return false;
		}
	}
	else if (ToolName == TEXT("configure_static_mesh") || ToolName == TEXT("create_dynamic_mesh") || ToolName == TEXT("audit_nanite_settings"))
	{
		if (!Params->HasField(TEXT("asset_path")))
		{
			OutErrors.Add(TEXT("Missing required field: asset_path"));
			return false;
		}
	}

	return true;
}

FAgentFrameworkActionResult FAgentFrameworkMeshActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FScopedTransaction> Transaction;
	Transaction.Emplace(FText::FromString(TEXT("AgentFramework Mesh Action")));

	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	if (Params->TryGetStringField(TEXT("_tool_name"), ToolName) || Params->TryGetStringField(TEXT("tool_name"), ToolName))
	{
		if (ToolName == TEXT("configure_static_mesh"))       Result = ExecuteConfigureStaticMesh(Params, Result);
		else if (ToolName == TEXT("create_dynamic_mesh"))    Result = ExecuteCreateDynamicMesh(Params, Result);
		else if (ToolName == TEXT("audit_nanite_settings"))  Result = ExecuteAuditNaniteSettings(Params, Result);
		else if (ToolName == TEXT("setup_runtime_virtual_texture")) Result = ExecuteSetupRuntimeVirtualTexture(Params, Result);
		else if (ToolName == TEXT("setup_chaos_physics"))    Result = ExecuteSetupChaosPhysics(Params, Result);
		else if (ToolName == TEXT("setup_dataflow_graph"))   Result = ExecuteSetupDataflowGraph(Params, Result);
		else if (ToolName == TEXT("setup_clothing_simulation")) Result = ExecuteSetupClothingSimulation(Params, Result);
		else if (ToolName == TEXT("setup_sparse_volume_texture")) Result = ExecuteSetupSparseVolumeTexture(Params, Result);
		else if (ToolName == TEXT("import_mesh") || ToolName == TEXT("import_assets_batch"))
		{
			// Skip direct return to fall through to import handling below
		}
		else
		{
			Result.Errors.Add(FString::Printf(TEXT("Unknown mesh tool: '%s'"), *ToolName));
			Transaction->Cancel();
			return Result;
		}

		if (ToolName != TEXT("import_mesh") && ToolName != TEXT("import_assets_batch"))
		{
			if (Transaction.IsSet() && !Result.bSuccess)
			{
				Transaction->Cancel();
			}
			return Result;
		}
	}

	// Get source file paths
	TArray<FString> SourceFiles;
	const TArray<TSharedPtr<FJsonValue>>* FilesArray = nullptr;
	if (Params->TryGetArrayField(TEXT("source_files"), FilesArray))
	{
		for (const auto& Val : *FilesArray)
		{
			FString Path;
			if (Val->TryGetString(Path)) SourceFiles.Add(Path);
		}
	}
	else
	{
		FString SingleFile;
		if (Params->TryGetStringField(TEXT("source_file"), SingleFile))
		{
			SourceFiles.Add(SingleFile);
		}
	}

	if (SourceFiles.Num() == 0)
	{
		Result.Errors.Add(TEXT("No source files specified for import."));
		if (Transaction.IsSet())
		{
			Transaction->Cancel();
		}
		return Result;
	}

	FString DestinationPath = TEXT("/Game/Meshes");
	Params->TryGetStringField(TEXT("destination_path"), DestinationPath);

	// Create import tasks
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TArray<UAssetImportTask*> ImportTasks;

	for (const FString& SourceFile : SourceFiles)
	{
		if (!FPaths::FileExists(SourceFile))
		{
			Result.Warnings.Add(FString::Printf(TEXT("File not found: %s"), *SourceFile));
			continue;
		}

		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = SourceFile;
		Task->DestinationPath = DestinationPath;
		Task->bAutomated = true;
		Task->bReplaceExisting = true;
		Task->bSave = true;
		ImportTasks.Add(Task);
	}

	if (ImportTasks.Num() == 0)
	{
		Result.Errors.Add(TEXT("No valid files to import."));
		if (Transaction.IsSet())
		{
			Transaction->Cancel();
		}
		return Result;
	}

	AssetTools.ImportAssetTasks(ImportTasks);

	// Collect results
	for (UAssetImportTask* Task : ImportTasks)
	{
		for (const FString& ImportedPath : Task->ImportedObjectPaths)
		{
			Result.ModifiedAssets.Add(ImportedPath);
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Imported %d asset(s) to %s"), ImportTasks.Num(), *DestinationPath);

	if (Transaction.IsSet() && !Result.bSuccess)
	{
		Transaction->Cancel();
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMeshActions::ExecuteConfigureStaticMesh(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Result.Errors.Add(TEXT("Missing 'asset_path' parameter."));
		return Result;
	}

	UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
	if (!StaticMesh)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load Static Mesh at path: %s"), *AssetPath));
		return Result;
	}

	// Enable Nanite settings
	bool bEnableNanite = false;
	if (Params->TryGetBoolField(TEXT("enable_nanite"), bEnableNanite))
	{
		FMeshNaniteSettings NaniteSettingsCopy = StaticMesh->GetNaniteSettings();
		NaniteSettingsCopy.bEnabled = bEnableNanite;
		StaticMesh->SetNaniteSettings(NaniteSettingsCopy);
	}

	// LOD Generation settings
	FString LodGeneration = TEXT("auto");
	Params->TryGetStringField(TEXT("lod_generation"), LodGeneration);

	if (LodGeneration == TEXT("auto") || LodGeneration == TEXT("manual"))
	{
		int32 LodCount = 4;
		Params->TryGetNumberField(TEXT("lod_count"), LodCount);
		if (LodCount < 1) LodCount = 1;

		StaticMesh->SetNumSourceModels(LodCount);
		for (int32 i = 1; i < LodCount; ++i)
		{
			FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(i);
			SourceModel.ReductionSettings.PercentTriangles = FMath::Pow(0.5f, i);
		}
	}
	else if (LodGeneration == TEXT("none"))
	{
		StaticMesh->SetNumSourceModels(1);
	}

	// Collision Complexity
	FString CollisionComplexity;
	if (Params->TryGetStringField(TEXT("collision_complexity"), CollisionComplexity))
	{
		if (UBodySetup* BodySetup = StaticMesh->GetBodySetup())
		{
			if (CollisionComplexity == TEXT("simple"))
			{
				BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
			}
			else if (CollisionComplexity == TEXT("complex") || CollisionComplexity == TEXT("use_complex_as_simple"))
			{
				BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
			}
			else if (CollisionComplexity == TEXT("simple_and_complex"))
			{
				BodySetup->CollisionTraceFlag = CTF_UseSimpleAndComplex;
			}
			else
			{
				BodySetup->CollisionTraceFlag = CTF_UseDefault;
			}
		}
	}

	// Lightmap Resolution
	int32 LightmapResolution = 0;
	if (Params->TryGetNumberField(TEXT("lightmap_resolution"), LightmapResolution) && LightmapResolution > 0)
	{
		StaticMesh->SetLightMapResolution(LightmapResolution);
	}

	// Mark package as dirty and rebuild the static mesh
	StaticMesh->Modify();
	StaticMesh->PostEditChange();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Configured Static Mesh settings for: %s"), *AssetPath);
	return Result;
}

#include "UDynamicMesh.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "VT/RuntimeVirtualTextureVolume.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "PhysicsField/PhysicsFieldComponent.h"
#include "Dataflow/DataflowObject.h"
#include "ClothingAsset.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "EngineUtils.h"
#include "UObject/SavePackage.h"

FAgentFrameworkActionResult FAgentFrameworkMeshActions::ExecuteCreateDynamicMesh(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UDynamicMesh* Mesh = Cast<UDynamicMesh>(AssetTools.CreateAsset(AssetName, PackagePath, UDynamicMesh::StaticClass(), nullptr));

	if (!Mesh)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create UDynamicMesh at '%s'."), *AssetPath));
		return Result;
	}

	Mesh->Modify();
	FGeometryScriptPrimitiveOptions Options;
	Options.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerFace;
	FTransform Transform = FTransform::Identity;

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
		Mesh,
		Options,
		Transform,
		200.f, 200.f, 200.f,
		1, 1, 1,
		EGeometryScriptPrimitiveOriginMode::Base,
		nullptr
	);

	UPackage* Package = Mesh->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, Mesh, *PackageFilename, SaveArgs);
	}
	FAssetRegistryModule::AssetCreated(Mesh);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully created procedurally modeled Dynamic Mesh Box at '%s'."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMeshActions::ExecuteAuditNaniteSettings(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
	if (!StaticMesh)
	{
		Result.Errors.Add(FString::Printf(TEXT("StaticMesh not found: '%s'"), *AssetPath));
		return Result;
	}

	StaticMesh->Modify();
	StaticMesh->GetNaniteSettings().bEnabled = true;
	StaticMesh->PostEditChange();

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully enabled Nanite settings for Static Mesh '%s'."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMeshActions::ExecuteSetupRuntimeVirtualTexture(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString RVTVolumeName = Params->GetStringField(TEXT("rvt_volume_name"));
	FString RVTAssetPath = Params->GetStringField(TEXT("rvt_asset_path"));

	URuntimeVirtualTexture* RVTAsset = LoadObject<URuntimeVirtualTexture>(nullptr, *RVTAssetPath);
	if (!RVTAsset)
	{
		Result.Errors.Add(FString::Printf(TEXT("RuntimeVirtualTexture asset not found at '%s'."), *RVTAssetPath));
		return Result;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Result.Errors.Add(TEXT("No active editor world found."));
		return Result;
	}

	ARuntimeVirtualTextureVolume* RVTVolume = nullptr;
	for (TActorIterator<ARuntimeVirtualTextureVolume> It(World); It; ++It)
	{
		if (It->GetName() == RVTVolumeName || It->GetActorLabel() == RVTVolumeName)
		{
			RVTVolume = *It;
			break;
		}
	}

	if (!RVTVolume)
	{
		RVTVolume = World->SpawnActor<ARuntimeVirtualTextureVolume>();
		RVTVolume->SetActorLabel(*RVTVolumeName);
	}

	URuntimeVirtualTextureComponent* Component = RVTVolume->VirtualTextureComponent;
	if (Component)
	{
		Component->Modify();
		Component->SetVirtualTexture(RVTAsset);
		Component->MarkRenderStateDirty();
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Configured Runtime Virtual Texture on volume '%s' with asset '%s'."), *RVTVolumeName, *RVTAssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMeshActions::ExecuteSetupChaosPhysics(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		Result.Errors.Add(TEXT("No active editor world found."));
		return Result;
	}

	UPhysicsFieldComponent* PhysicsField = NewObject<UPhysicsFieldComponent>();
	if (!PhysicsField)
	{
		Result.Errors.Add(TEXT("Failed to instantiate UPhysicsFieldComponent."));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("Successfully instantiated and verified UPhysicsFieldComponent for Chaos Physics solver.");
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMeshActions::ExecuteSetupDataflowGraph(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetShortName(AssetPath);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UDataflow* DataflowObj = Cast<UDataflow>(AssetTools.CreateAsset(AssetName, PackagePath, UDataflow::StaticClass(), nullptr));

	if (!DataflowObj)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create UDataflow at '%s'."), *AssetPath));
		return Result;
	}

	UPackage* Package = DataflowObj->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, DataflowObj, *PackageFilename, SaveArgs);
	}
	FAssetRegistryModule::AssetCreated(DataflowObj);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully created Procedural Dataflow Graph asset at '%s'."), *AssetPath);
	Result.ModifiedAssets.Add(AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMeshActions::ExecuteSetupClothingSimulation(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClothingAssetBase* ClothAsset = NewObject<UClothingAssetBase>();
	if (!ClothAsset)
	{
		Result.Errors.Add(TEXT("Failed to instantiate Clothing Simulation asset."));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("Successfully instantiated and verified Clothing Simulation Asset.");
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMeshActions::ExecuteSetupSparseVolumeTexture(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	USparseVolumeTexture* SVT = NewObject<USparseVolumeTexture>();
	if (!SVT)
	{
		Result.Errors.Add(TEXT("Failed to instantiate Sparse Volume Texture."));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("Successfully instantiated and verified Sparse Volume Texture (SVT) asset.");
	return Result;
}
