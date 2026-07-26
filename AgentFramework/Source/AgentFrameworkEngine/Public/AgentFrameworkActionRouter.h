// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkTypes.h"
#include "AgentFrameworkInterfaces.h"
#include "HAL/CriticalSection.h"
#include "Misc/Guid.h"

/**
 * Handle representing an asynchronous tool execution task.
 */
struct AGENTFRAMEWORKENGINE_API FAgentFrameworkAsyncTaskHandle
{
	FGuid TaskId;
	FAgentFrameworkToolCall ToolCall;
	TFunction<void(FAgentFrameworkActionResult)> OnComplete;
	double EnqueueTime = 0.0;
};

DECLARE_MULTICAST_DELEGATE_FiveParams(FOnToolExecutionRecorded, const FString& /*ToolName*/, double /*DurationMicros*/, bool /*bSuccess*/, const TArray<FString>& /*Errors*/, const FString& /*ContextSummary*/);

/**
 * Routes tool calls from the AI to the appropriate action executor.
 * Supports synchronous routing and thread-safe non-blocking asynchronous queueing.
 */
class AGENTFRAMEWORKENGINE_API FAgentFrameworkActionRouter : public TSharedFromThis<FAgentFrameworkActionRouter>
{
public:
	FAgentFrameworkActionRouter();
	~FAgentFrameworkActionRouter();

	/** Delegate fired whenever a tool call is executed and recorded for telemetry */
	static FOnToolExecutionRecorded OnToolExecutionRecorded;

	/** Register an action executor */
	void RegisterExecutor(TSharedRef<IAgentFrameworkActionExecutor> Executor);

	/** Clear all registered executors */
	void ClearExecutors();

	/** Route a tool call synchronously to the appropriate executor (must be on Game Thread) */
	FAgentFrameworkActionResult RouteToolCall(const FAgentFrameworkToolCall& ToolCall);

	/**
	 * Route a tool call asynchronously.
	 * Thread-safely enqueues the tool call and schedules Game Thread execution.
	 * Calls OnComplete on the Game Thread once execution finishes.
	 * @return Unique handle TaskId for tracking or cancellation.
	 */
	FGuid RouteToolCallAsync(const FAgentFrameworkToolCall& ToolCall, TFunction<void(FAgentFrameworkActionResult)> OnComplete);

	/** Get number of tasks currently waiting in the async queue */
	int32 GetPendingTaskCount() const;

	/** Attempt to cancel a pending async task by TaskId (returns true if cancelled before execution) */
	bool CancelTask(const FGuid& TaskId);

	/** Clear and notify all pending tasks */
	void ClearPendingTasks();

	/** Find executor by tool name */
	TSharedPtr<IAgentFrameworkActionExecutor> FindExecutorForTool(const FString& ToolName) const;

private:
	/** Process enqueued tasks on the Game Thread */
	void ProcessTaskQueue();

	/** Map of tool name -> executor */
	TMap<FString, TSharedRef<IAgentFrameworkActionExecutor>> ExecutorMap;

	/** Critical section for thread-safe task queue access */
	mutable FCriticalSection TaskQueueCS;

	/** Queue of pending async tasks */
	TArray<FAgentFrameworkAsyncTaskHandle> PendingTasks;
};

