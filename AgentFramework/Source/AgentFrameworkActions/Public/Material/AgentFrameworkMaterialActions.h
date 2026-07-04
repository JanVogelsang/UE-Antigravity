// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

class AGENTFRAMEWORKACTIONS_API FAgentFrameworkMaterialActions : public IAgentFrameworkActionExecutor
{
public:
    FAgentFrameworkMaterialActions();
    virtual ~FAgentFrameworkMaterialActions();

    virtual FName GetActionName() const override;
    virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
    virtual TArray<FString> GetSupportedToolNames() const override;
    virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
    FAgentFrameworkActionResult ExecuteCreateMaterial(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
    FAgentFrameworkActionResult ExecuteCreateMaterialInstance(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
    FAgentFrameworkActionResult ExecuteAddMaterialExpression(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
    FAgentFrameworkActionResult ExecuteCaptureMaterial(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
};
