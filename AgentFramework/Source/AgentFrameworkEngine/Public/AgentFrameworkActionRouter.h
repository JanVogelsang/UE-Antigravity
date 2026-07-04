// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkTypes.h"
#include "AgentFrameworkInterfaces.h"

/**
 * Routes tool calls from the AI to the appropriate action executor.
 */
class AGENTFRAMEWORKENGINE_API FAgentFrameworkActionRouter
{
public:
	FAgentFrameworkActionRouter();
	~FAgentFrameworkActionRouter();

	/** Register an action executor */
	void RegisterExecutor(TSharedRef<IAgentFrameworkActionExecutor> Executor);

	/** Clear all registered executors */
	void ClearExecutors();

	/** Route a tool call to the appropriate executor */
	FAgentFrameworkActionResult RouteToolCall(const FAgentFrameworkToolCall& ToolCall);



	/** Find executor by tool name */
	TSharedPtr<IAgentFrameworkActionExecutor> FindExecutorForTool(const FString& ToolName) const;

private:
	/** Map of tool name -> executor */
	TMap<FString, TSharedRef<IAgentFrameworkActionExecutor>> ExecutorMap;
};
