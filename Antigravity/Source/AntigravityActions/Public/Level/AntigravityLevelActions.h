// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

class UWorld;

class ANTIGRAVITYACTIONS_API FAntigravityLevelActions : public IAntigravityActionExecutor
{
public:
    FAntigravityLevelActions();
    virtual ~FAntigravityLevelActions();

    virtual FName GetActionName() const override;
    virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
    virtual TArray<FString> GetSupportedToolNames() const override;
    virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
    FAntigravityActionResult ExecuteSpawnActor(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAntigravityActionResult ExecutePlaceLight(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAntigravityActionResult ExecuteModifyWorldSettings(const TSharedRef<FJsonObject>& Params, UWorld* World);
};
