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

    FAgentFrameworkActionResult ExecuteConfigureWorldPartition(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecuteCreateFoliageType(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecutePaintFoliageBrush(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecuteCreateLandscape(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecuteCreateLandscapeGrassType(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecuteCreateLevelInstance(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecuteCreatePackedLevelActor(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecuteSetupCineCameraRigRail(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecuteSetupDMXPatch(const TSharedRef<FJsonObject>& Params, UWorld* World);
    FAgentFrameworkActionResult ExecuteSetupChaosVehicle(const TSharedRef<FJsonObject>& Params, UWorld* World);
};
