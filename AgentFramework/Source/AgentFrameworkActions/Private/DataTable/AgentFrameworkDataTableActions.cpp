// Copyright 2026 AgentFramework. All Rights Reserved.

#include "DataTable/AgentFrameworkDataTableActions.h"
#include "AgentFrameworkActionUtils.h"
#include "Engine/DataTable.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataTableFactory.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Sound/SoundBase.h"
#endif

#define LOCTEXT_NAMESPACE "AgentFrameworkDataTableActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAgentFrameworkDataTableActions::FAgentFrameworkDataTableActions() {}
FAgentFrameworkDataTableActions::~FAgentFrameworkDataTableActions() {}

// ============================================================================
// IAgentFrameworkActionExecutor Interface
// ============================================================================

FName FAgentFrameworkDataTableActions::GetActionName() const
{
	return FName(TEXT("DataTable"));
}

TArray<FString> FAgentFrameworkDataTableActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_data_table"),
		TEXT("import_json_to_datatable")
	};
}

bool FAgentFrameworkDataTableActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString Action;
	TArray<FString> TempErrors;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), Action, TempErrors, false) && !Action.IsEmpty())
	{
		if (Action != TEXT("create_data_table") && Action != TEXT("import_json_to_datatable"))
		{
			OutErrors.Add(FString::Printf(TEXT("Unknown action: %s"), *Action));
			return false;
		}
	}
	return OutErrors.Num() == 0;
}

FAgentFrameworkActionResult FAgentFrameworkDataTableActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString Action;
	TArray<FString> TempErrors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), Action, TempErrors, false) || Action.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), Action, TempErrors, false);
	}

	if (Action == TEXT("create_data_table"))
	{
		Result = ExecuteCreateDataTable(Params, Result);
	}
	else if (Action == TEXT("import_json_to_datatable"))
	{
		Result = ExecuteImportJsonToDataTable(Params, Result);
	}
	else
	{
		FString DummyRowStruct;
		if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("row_struct"), DummyRowStruct, TempErrors, false) && !DummyRowStruct.IsEmpty())
		{
			Result = ExecuteCreateDataTable(Params, Result);
		}
		else
		{
			FString DummyJsonData;
			if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("json_data"), DummyJsonData, TempErrors, false) && !DummyJsonData.IsEmpty())
			{
				Result = ExecuteImportJsonToDataTable(Params, Result);
			}
			else
			{
				Result.Errors.Add(TEXT("Could not determine DataTable action. Provide 'action' field or appropriate parameters."));
			}
		}
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	return Result;
}

// ============================================================================
// create_data_table
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkDataTableActions::ExecuteCreateDataTable(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString RowStructName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("row_struct"), RowStructName, Result.Errors, true))
	{
		return Result;
	}

	// Resolve the row struct by name
	UScriptStruct* RowStruct = FindFirstObject<UScriptStruct>(*RowStructName, EFindFirstObjectOptions::NativeFirst);
	if (!IsValid(RowStruct))
	{
		// Try with the 'F' prefix
		FString WithF = TEXT("F") + RowStructName;
		RowStruct = FindFirstObject<UScriptStruct>(*WithF, EFindFirstObjectOptions::NativeFirst);
	}

	if (!IsValid(RowStruct))
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Row struct '%s' not found. Ensure it derives from FTableRowBase and is compiled. "
				 "For built-in UE structs, use the full name (e.g., 'FCharacterInfoRow')."),
			*RowStructName));
		return Result;
	}

	// Verify it derives from FTableRowBase
	UScriptStruct* TableRowBaseStruct = FTableRowBase::StaticStruct();
	if (!IsValid(TableRowBaseStruct) || !RowStruct->IsChildOf(TableRowBaseStruct))
	{
		Result.Errors.Add(FString::Printf(
			TEXT("Struct '%s' does not derive from FTableRowBase. DataTable row structs must inherit from FTableRowBase."),
			*RowStructName));
		return Result;
	}

	// Create the DataTable using the factory
	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	if (!IsValid(Factory))
	{
		Result.Errors.Add(TEXT("Failed to create DataTableFactory."));
		return Result;
	}
	Factory->Struct = RowStruct;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UDataTable::StaticClass(), Factory);
	if (!IsValid(NewAsset))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create DataTable at '%s'. Check that the path is valid."), *AssetPath));
		return Result;
	}

	UDataTable* DataTable = Cast<UDataTable>(NewAsset);
	if (!IsValid(DataTable))
	{
		Result.Errors.Add(TEXT("Failed to cast created asset to UDataTable."));
		return Result;
	}

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(
		TEXT("Created DataTable '%s' with row struct '%s'. "
			 "The table is empty — use import_json_to_datatable to populate it with rows."),
		*AssetPath, *RowStruct->GetName());

	return Result;
}

// ============================================================================
// import_json_to_datatable
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkDataTableActions::ExecuteImportJsonToDataTable(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString JsonDataStr;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("json_data"), JsonDataStr, Result.Errors, true))
	{
		return Result;
	}

	// Load the DataTable
	UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *AssetPath);
	if (!IsValid(DataTable))
	{
		Result.Errors.Add(FString::Printf(TEXT("DataTable not found at '%s'. Create it first with create_data_table."), *AssetPath));
		return Result;
	}

	// Parse the JSON — expect an array of objects, each with a "Name" key
	TArray<TSharedPtr<FJsonValue>> JsonRows;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonDataStr);

	if (!FJsonSerializer::Deserialize(Reader, JsonRows) || JsonRows.Num() == 0)
	{
		Result.Errors.Add(TEXT("Failed to parse 'json_data' as a JSON array. Expected format: [{\"Name\": \"Row_1\", ...}, ...]"));
		return Result;
	}

	// Convert to the format UDataTable::CreateTableFromJSONString expects
	FString FormattedJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&FormattedJson);
	FJsonSerializer::Serialize(JsonRows, Writer);

	// Use DataTable's built-in JSON import
	DataTable->Modify();
	DataTable->EmptyTable();

	TArray<FString> ImportErrors = DataTable->CreateTableFromJSONString(FormattedJson);

	if (ImportErrors.Num() > 0)
	{
		for (const FString& Err : ImportErrors)
		{
			Result.Warnings.Add(FString::Printf(TEXT("Import note: %s"), *Err));
		}
	}

	int32 RowCount = DataTable->GetRowMap().Num();

	if (RowCount == 0)
	{
		// Fallback: try to manually add rows if the bulk import failed
		Result.Errors.Add(FString::Printf(
			TEXT("DataTable import produced 0 rows. Ensure the JSON matches the struct '%s'. "
				 "Each object must have a 'Name' field for the row key, and property names must match "
				 "the struct's UPROPERTY names exactly."),
			*DataTable->GetRowStructPathName().ToString()));
		return Result;
	}

	// Mark dirty for save
	DataTable->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(
		TEXT("Successfully imported %d rows into DataTable '%s'."),
		RowCount, *AssetPath);

	return Result;
}

void FAgentFrameworkDataTableActions::PlaySuccessSound()
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

#undef LOCTEXT_NAMESPACE

