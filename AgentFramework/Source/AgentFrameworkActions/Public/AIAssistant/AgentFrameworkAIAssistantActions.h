// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"
#include "UObject/StrongObjectPtr.h"

class UAIAssistantBridge;

/**
 * FAgentFrameworkAIAssistantActions
 *
 * Bridge tools to query Epic's native AIAssistant:
 *   - query_epic_assistant: Send a query to the Epic AI Assistant
 */
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkAIAssistantActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkAIAssistantActions();
	virtual ~FAgentFrameworkAIAssistantActions();

	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

	static UAIAssistantBridge* GetBridgeInstance();

private:
	static TStrongObjectPtr<UAIAssistantBridge> BridgeInstance;
};
