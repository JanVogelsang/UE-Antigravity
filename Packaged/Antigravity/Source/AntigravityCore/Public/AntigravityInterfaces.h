// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityTypes.h"

/**
 * Interface for all action executors.
 * Each action executor handles a specific domain of UE operations
 * (Blueprints, Materials, C++, etc.)
 */
class IAntigravityActionExecutor
{
public:
	virtual ~IAntigravityActionExecutor() = default;

	/** Get the unique name of this action executor */
	virtual FName GetActionName() const = 0;

	/** Get the display name for UI */
	virtual FText GetDisplayName() const = 0;

	/** Get the category this executor belongs to */
	virtual EAntigravityActionCategory GetCategory() const = 0;

	/** Get the default risk level for this action type */
	virtual EAntigravityRiskLevel GetDefaultRiskLevel() const = 0;

	/**
	 * Preview the action without executing it.
	 * Returns a plan describing what will happen, including affected files/assets.
	 */
	virtual FAntigravityActionPlan PreviewAction(const TSharedRef<FJsonObject>& Params) = 0;

	/**
	 * Execute the action.
	 * Should be called only after the user has approved the plan.
	 */
	virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) = 0;

	/** Whether this action can be undone */
	virtual bool CanUndo() const = 0;

	/** Undo the last executed action */
	virtual bool UndoAction() = 0;

	/** Get the list of tool names this executor handles */
	virtual TArray<FString> GetSupportedToolNames() const = 0;

	/** Validate input parameters before execution */
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const = 0;
};


