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

	/** Preview a tool call without executing */
	FAntigravityActionPlan PreviewToolCall(const FAntigravityToolCall& ToolCall);

	/** Get all registered executor names */
	TArray<FName> GetRegisteredExecutorNames() const;

	/** Get all registered tool names (keys of the executor map) */
	TArray<FString> GetRegisteredToolNames() const;

	/** Find executor by tool name */
	TSharedPtr<IAntigravityActionExecutor> FindExecutorForTool(const FString& ToolName) const;

private:
	/** Map of tool name -> executor */
	TMap<FString, TSharedRef<IAntigravityActionExecutor>> ExecutorMap;
};
