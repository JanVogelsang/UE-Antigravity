// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

class ANTIGRAVITYACTIONS_API FAntigravityMaterialActions : public IAntigravityActionExecutor
{
public:
    FAntigravityMaterialActions();
    virtual ~FAntigravityMaterialActions();

    virtual FName GetActionName() const override;
    virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
    virtual TArray<FString> GetSupportedToolNames() const override;
    virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
    FAntigravityActionResult ExecuteCreateMaterial(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
    FAntigravityActionResult ExecuteCreateMaterialInstance(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
    FAntigravityActionResult ExecuteAddMaterialExpression(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
    FAntigravityActionResult ExecuteCaptureMaterial(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
};
