// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

class ANTIGRAVITYACTIONS_API FAntigravityMeshActions : public IAntigravityActionExecutor
{
public:
    FAntigravityMeshActions();
    virtual ~FAntigravityMeshActions();

    virtual FName GetActionName() const override;
    virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
    virtual TArray<FString> GetSupportedToolNames() const override;
    virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
    FAntigravityActionResult ExecuteConfigureStaticMesh(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
};
