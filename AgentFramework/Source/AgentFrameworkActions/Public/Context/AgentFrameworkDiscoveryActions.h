// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

/**
 * FAgentFrameworkDiscoveryActions
 * 
 * Provides on-demand discovery tools for the active MCP toolset:
 *   - get_tool_info: Load the full schema for a specific tool.
 *   - list_tools_in_category: List all available tools in a category with brief descriptions.
 */
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkDiscoveryActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkDiscoveryActions();
	virtual ~FAgentFrameworkDiscoveryActions();

	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	FAgentFrameworkActionResult ExecuteGetToolInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteListToolsInCategory(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
};
