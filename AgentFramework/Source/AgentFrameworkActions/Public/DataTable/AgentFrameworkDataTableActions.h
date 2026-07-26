// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

/**
 * FAgentFrameworkDataTableActions
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
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkDataTableActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkDataTableActions();
	virtual ~FAgentFrameworkDataTableActions();

	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/** Create a new DataTable asset with a specified row struct. */
	FAgentFrameworkActionResult ExecuteCreateDataTable(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Import JSON data into an existing or new DataTable. */
	FAgentFrameworkActionResult ExecuteImportJsonToDataTable(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Play success editor sound when DataTable action completes successfully. */
	void PlaySuccessSound();
};
