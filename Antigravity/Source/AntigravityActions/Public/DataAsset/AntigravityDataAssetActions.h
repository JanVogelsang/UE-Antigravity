// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * FAntigravityDataAssetActions
 *
 * Provides tools for creating, updating, and inspecting UDataAsset / UPrimaryDataAsset assets:
 *   - create_data_asset: Create a Data Asset of a specified class
 *   - set_data_asset_properties: Write properties on a Data Asset using reflection
 *   - get_data_asset_info: Inspect a Data Asset's current properties and values
 */
class ANTIGRAVITYACTIONS_API FAntigravityDataAssetActions : public IAntigravityActionExecutor
{
public:
	FAntigravityDataAssetActions();
	virtual ~FAntigravityDataAssetActions();

	virtual FName GetActionName() const override;
	virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	FAntigravityActionResult ExecuteCreateDataAsset(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
	FAntigravityActionResult ExecuteSetDataAssetProperties(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
	FAntigravityActionResult ExecuteGetDataAssetInfo(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
};
