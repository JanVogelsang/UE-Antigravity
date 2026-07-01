// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * FAntigravityDiscoveryActions
 * 
 * Provides on-demand discovery tools for the active MCP toolset:
 *   - get_tool_info: Load the full schema for a specific tool.
 *   - list_tools_in_category: List all available tools in a category with brief descriptions.
 */
class ANTIGRAVITYACTIONS_API FAntigravityDiscoveryActions : public IAntigravityActionExecutor
{
public:
	FAntigravityDiscoveryActions();
	virtual ~FAntigravityDiscoveryActions();

	virtual FName GetActionName() const override;
	virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	FAntigravityActionResult ExecuteGetToolInfo(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
	FAntigravityActionResult ExecuteListToolsInCategory(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
};
