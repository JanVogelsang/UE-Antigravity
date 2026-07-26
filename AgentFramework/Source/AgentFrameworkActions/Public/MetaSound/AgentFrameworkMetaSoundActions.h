// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"
#include "Dom/JsonObject.h"

/**
 * FAgentFrameworkMetaSoundActions
 *
 * Native C++ Action Executor for MetaSound audio asset creation and graph wiring.
 * Implements tools:
 * - create_metasound_source
 * - wire_metasound_nodes
 */
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkMetaSoundActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkMetaSoundActions();
	virtual ~FAgentFrameworkMetaSoundActions();

	// IAgentFrameworkActionExecutor Interface
	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	FAgentFrameworkActionResult ExecuteCreateMetaSoundSource(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteWireMetaSoundNodes(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	void PlaySuccessSound();
};
