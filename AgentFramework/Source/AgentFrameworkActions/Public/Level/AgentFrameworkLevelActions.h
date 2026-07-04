// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

class UWorld;

class AGENTFRAMEWORKACTIONS_API FAgentFrameworkLevelActions : public IAgentFrameworkActionExecutor
{
public:
    FAgentFrameworkLevelActions();
    virtual ~FAgentFrameworkLevelActions();

    virtual FName GetActionName() const override;
    virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
    virtual TArray<FString> GetSupportedToolNames() const override;
    virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
    FAgentFrameworkActionResult ExecuteSpawnActor(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecutePlaceLight(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecuteModifyWorldSettings(const TSharedRef<FJsonObject>& Params, UWorld* World);
};
