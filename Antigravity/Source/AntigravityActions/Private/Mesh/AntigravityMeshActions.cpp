// Copyright 2026 Antigravity. All Rights Reserved.

#include "Mesh/AntigravityMeshActions.h"
#include "AntigravityCoreModule.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "Factories/FbxFactory.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"

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
	FAntigravityActionResult Result;
	Result.bSuccess = false;

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
	return Result;
}
