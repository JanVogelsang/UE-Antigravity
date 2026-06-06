// Copyright 2026 Antigravity. All Rights Reserved.

#include "Mesh/AntigravityMeshActions.h"
#include "AntigravityCoreModule.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "Factories/FbxFactory.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "ScopedTransaction.h"

FAntigravityMeshActions::FAntigravityMeshActions() {}
FAntigravityMeshActions::~FAntigravityMeshActions() {}
FName FAntigravityMeshActions::GetActionName() const { return FName(TEXT("Mesh")); }
FText FAntigravityMeshActions::GetDisplayName() const { return FText::FromString(TEXT("Mesh Actions")); }
EAntigravityActionCategory FAntigravityMeshActions::GetCategory() const { return EAntigravityActionCategory::Mesh; }
EAntigravityRiskLevel FAntigravityMeshActions::GetDefaultRiskLevel() const { return EAntigravityRiskLevel::Low; }
bool FAntigravityMeshActions::CanUndo() const { return false; }
bool FAntigravityMeshActions::UndoAction() { return false; }

TArray<FString> FAntigravityMeshActions::GetSupportedToolNames() const
{
	return {
		TEXT("import_mesh"),
		TEXT("import_assets_batch"),
		TEXT("configure_static_mesh")
	};
}

bool FAntigravityMeshActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
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
	else if (ToolName == TEXT("configure_static_mesh"))
	{
		if (!Params->HasField(TEXT("asset_path")))
		{
			OutErrors.Add(TEXT("Missing required field for configure_static_mesh: asset_path"));
			return false;
		}
	}

	return true;
}

FAntigravityActionPlan FAntigravityMeshActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionPlan Plan;
	Plan.Summary = TEXT("Mesh/Asset import operation");
	FAntigravityAction Action;
	Action.Description = Plan.Summary;
	Action.Category = EAntigravityActionCategory::Mesh;
	Action.RiskLevel = EAntigravityRiskLevel::Low;
	Plan.Actions.Add(Action);
	return Plan;
}

FAntigravityActionResult FAntigravityMeshActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	TOptional<FScopedTransaction> Transaction;
	Transaction.Emplace(FText::FromString(TEXT("Antigravity Mesh Action")));

	FAntigravityActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	if (Params->TryGetStringField(TEXT("_tool_name"), ToolName) || Params->TryGetStringField(TEXT("tool_name"), ToolName))
	{
		if (ToolName == TEXT("configure_static_mesh"))
		{
			Result = ExecuteConfigureStaticMesh(Params, Result);
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

FAntigravityActionResult FAntigravityMeshActions::ExecuteConfigureStaticMesh(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
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
