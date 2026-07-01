// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * FAntigravityDataTableActions
 *
 * Provides tools for creating and populating DataTable assets:
 *   - create_data_table: Create a DataTable from an existing row struct
 *   - import_json_to_datatable: Populate a DataTable from inline JSON data
 *
 * DataTables are used extensively in UE for game balancing, item databases,
 * weapon stats, dialogue, level configuration, and more.
 *
 * The AI can:
 *   - Create DataTables targeting any FTableRowBase-derived struct
 *   - Generate balanced game data (e.g., RPG weapon progression curves)
 *   - Import pre-computed JSON data directly into rows
 *   - Modify existing DataTable rows
 */
class ANTIGRAVITYACTIONS_API FAntigravityDataTableActions : public IAntigravityActionExecutor
{
public:
	FAntigravityDataTableActions();
	virtual ~FAntigravityDataTableActions();

	virtual FName GetActionName() const override;
	virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/** Create a new DataTable asset with a specified row struct. */
	FAntigravityActionResult ExecuteCreateDataTable(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Import JSON data into an existing or new DataTable. */
	FAntigravityActionResult ExecuteImportJsonToDataTable(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
};
