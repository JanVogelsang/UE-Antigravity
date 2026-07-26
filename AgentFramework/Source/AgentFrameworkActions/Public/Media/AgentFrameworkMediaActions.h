// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"
#include "Dom/JsonObject.h"

/** Action delegate callback fired when a media action completes successfully. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FAgentFrameworkOnMediaActionCompleted, FName /*ActionName*/, const FAgentFrameworkActionResult& /*Result*/);

class AGENTFRAMEWORKACTIONS_API FAgentFrameworkMediaActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkMediaActions();
	virtual ~FAgentFrameworkMediaActions();

	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

	/** Delegate triggered upon successful execution of a media action. */
	FAgentFrameworkOnMediaActionCompleted OnMediaActionCompleted;

private:
	FAgentFrameworkActionResult ExecuteCreateMediaPlayer(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteCreateMediaTexture(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteCreateFileMediaSource(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteConfigureMediaPlayer(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteGetMediaInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteConfigureSoundWaveCue(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	void PlaySuccessSound();
};
