// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityTypes.h"
#include "AntigravityInterfaces.h"

/**
 * Routes tool calls from the AI to the appropriate action executor.
 */
class ANTIGRAVITYENGINE_API FAntigravityActionRouter
{
public:
	FAntigravityActionRouter();
	~FAntigravityActionRouter();

	/** Register an action executor */
	void RegisterExecutor(TSharedRef<IAntigravityActionExecutor> Executor);

	/** Clear all registered executors */
	void ClearExecutors();

	/** Route a tool call to the appropriate executor */
	FAntigravityActionResult RouteToolCall(const FAntigravityToolCall& ToolCall);



	/** Find executor by tool name */
	TSharedPtr<IAntigravityActionExecutor> FindExecutorForTool(const FString& ToolName) const;

private:
	/** Map of tool name -> executor */
	TMap<FString, TSharedRef<IAntigravityActionExecutor>> ExecutorMap;
};
