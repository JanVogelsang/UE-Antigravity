// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityTaskDelegation.h"

FAntigravityTaskDelegation::FAntigravityTaskDelegation()
{
}

FAntigravityTaskDelegation::~FAntigravityTaskDelegation()
{
}

bool FAntigravityTaskDelegation::CreateSubTask(
	const FString& ParentTabId,
	EAntigravityAgentMode Mode,
	const FString& Message,
	const FString& InitialTodos,
	FAntigravitySubTask& OutSubTask
)
{
	// Check nesting depth
	if (GetNestingDepth(ParentTabId) >= MaxNestingDepth)
	{
		UE_LOG(LogTemp, Warning, TEXT("AntigravityTaskDelegation: Max nesting depth (%d) reached for tab %s"),
			MaxNestingDepth, *ParentTabId);
		return false;
	}

	// Check if parent already has an active child
	if (HasActiveChildTask(ParentTabId))
	{
		UE_LOG(LogTemp, Warning, TEXT("AntigravityTaskDelegation: Tab %s already has an active child task"),
			*ParentTabId);
		return false;
	}

	// Create the sub-task record
	FAntigravitySubTask NewSubTask;
	NewSubTask.ParentTabId = ParentTabId;
	NewSubTask.Mode = Mode;
	NewSubTask.Message = Message;
	NewSubTask.InitialTodos = InitialTodos;
	NewSubTask.Status = EAntigravitySubTaskStatus::Pending;

	// The ChildTabId will be set by SAntigravityMainPanel after creating the new tab
	// For now, leave it empty â€” it will be set via SetChildTabId()

	SubTasks.Add(NewSubTask);
	OutSubTask = NewSubTask;

	UE_LOG(LogTemp, Log, TEXT("AntigravityTaskDelegation: Created sub-task %s for parent tab %s (mode: %d)"),
		*NewSubTask.SubTaskId, *ParentTabId, (int32)Mode);

	return true;
}

void FAntigravityTaskDelegation::OnChildTaskCompleted(
	const FString& ChildTabId,
	bool bSuccess,
	const FString& ResultMessage
)
{
	// Find the sub-task associated with this child tab
	FString* SubTaskIdPtr = ChildTabToSubTaskMap.Find(ChildTabId);
	if (!SubTaskIdPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("AntigravityTaskDelegation: No sub-task found for child tab %s"), *ChildTabId);
		return;
	}

	// Find and update the sub-task
	for (FAntigravitySubTask& SubTask : SubTasks)
	{
		if (SubTask.SubTaskId == *SubTaskIdPtr)
		{
			SubTask.Status = bSuccess ? EAntigravitySubTaskStatus::Completed : EAntigravitySubTaskStatus::Failed;
			SubTask.ResultMessage = ResultMessage;
			SubTask.CompletedAt = FDateTime::UtcNow();

			// Remove from active child map
			ParentToActiveChildMap.Remove(SubTask.ParentTabId);

			UE_LOG(LogTemp, Log, TEXT("AntigravityTaskDelegation: Sub-task %s completed (success=%d) for parent tab %s"),
				*SubTask.SubTaskId, bSuccess, *SubTask.ParentTabId);

			// Fire completion delegate
			OnSubTaskCompleted.ExecuteIfBound(SubTask.SubTaskId, bSuccess, ResultMessage);
			return;
		}
	}
}

void FAntigravityTaskDelegation::CancelSubTask(const FString& SubTaskId)
{
	for (FAntigravitySubTask& SubTask : SubTasks)
	{
		if (SubTask.SubTaskId == SubTaskId)
		{
			SubTask.Status = EAntigravitySubTaskStatus::Cancelled;
			SubTask.CompletedAt = FDateTime::UtcNow();

			ParentToActiveChildMap.Remove(SubTask.ParentTabId);

			if (!SubTask.ChildTabId.IsEmpty())
			{
				ChildTabToSubTaskMap.Remove(SubTask.ChildTabId);
			}

			UE_LOG(LogTemp, Log, TEXT("AntigravityTaskDelegation: Cancelled sub-task %s"), *SubTaskId);
			return;
		}
	}
}

const FAntigravitySubTask* FAntigravityTaskDelegation::GetSubTask(const FString& SubTaskId) const
{
	for (const FAntigravitySubTask& SubTask : SubTasks)
	{
		if (SubTask.SubTaskId == SubTaskId)
		{
			return &SubTask;
		}
	}
	return nullptr;
}

TArray<FAntigravitySubTask*> FAntigravityTaskDelegation::GetActiveSubTasksForTab(const FString& ParentTabId)
{
	TArray<FAntigravitySubTask*> Active;
	for (FAntigravitySubTask& SubTask : SubTasks)
	{
		if (SubTask.ParentTabId == ParentTabId &&
			(SubTask.Status == EAntigravitySubTaskStatus::Pending ||
			 SubTask.Status == EAntigravitySubTaskStatus::Active))
		{
			Active.Add(&SubTask);
		}
	}
	return Active;
}

const FAntigravitySubTask* FAntigravityTaskDelegation::GetSubTaskByChildTab(const FString& ChildTabId) const
{
	const FString* SubTaskIdPtr = ChildTabToSubTaskMap.Find(ChildTabId);
	if (!SubTaskIdPtr) return nullptr;

	for (const FAntigravitySubTask& SubTask : SubTasks)
	{
		if (SubTask.SubTaskId == *SubTaskIdPtr)
		{
			return &SubTask;
		}
	}
	return nullptr;
}

bool FAntigravityTaskDelegation::SetChildTabId(const FString& SubTaskId, const FString& ChildTabId)
{
	for (FAntigravitySubTask& SubTask : SubTasks)
	{
		if (SubTask.SubTaskId == SubTaskId)
		{
			SubTask.ChildTabId = ChildTabId;
			SubTask.Status = EAntigravitySubTaskStatus::Active;

			ChildTabToSubTaskMap.Add(ChildTabId, SubTaskId);
			ParentToActiveChildMap.Add(SubTask.ParentTabId, ChildTabId);

			UE_LOG(LogTemp, Log, TEXT("AntigravityTaskDelegation: Linked child tab %s to sub-task %s"),
				*ChildTabId, *SubTaskId);
			return true;
		}
	}
	return false;
}

bool FAntigravityTaskDelegation::HasActiveChildTask(const FString& TabId) const
{
	return ParentToActiveChildMap.Contains(TabId);
}

int32 FAntigravityTaskDelegation::GetNestingDepth(const FString& TabId) const
{
	// Walk up the parent chain
	int32 Depth = 0;
	FString CurrentTabId = TabId;

	while (Depth < MaxNestingDepth)
	{
		// Find if this tab is a child of some parent
		bool bFoundParent = false;
		for (const FAntigravitySubTask& SubTask : SubTasks)
		{
			if (SubTask.ChildTabId == CurrentTabId)
			{
				CurrentTabId = SubTask.ParentTabId;
				Depth++;
				bFoundParent = true;
				break;
			}
		}
		if (!bFoundParent) break;
	}

	return Depth;
}
