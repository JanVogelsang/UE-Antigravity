// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkTypes.h"

/**
 * Interface for all action executors.
 * Each action executor handles a specific domain of UE operations
 * (Blueprints, Materials, C++, etc.)
 */
class IAgentFrameworkActionExecutor
{
public:
	virtual ~IAgentFrameworkActionExecutor() = default;

	/** Get the unique name of this action executor */
	virtual FName GetActionName() const = 0;

	/**
	 * Execute the action.
	 * Should be called only after the user has approved the plan.
	 */
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) = 0;

	/** Get the list of tool names this executor handles */
	virtual TArray<FString> GetSupportedToolNames() const = 0;

	/** Validate input parameters before execution */
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const = 0;
};


