// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

class AGENTFRAMEWORKACTIONS_API FAgentFrameworkMeshActions : public IAgentFrameworkActionExecutor
{
public:
    FAgentFrameworkMeshActions();
    virtual ~FAgentFrameworkMeshActions();

    virtual FName GetActionName() const override;
    virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
    virtual TArray<FString> GetSupportedToolNames() const override;
    virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
    FAgentFrameworkActionResult ExecuteConfigureStaticMesh(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
};
