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



/**
 * Interface for the context gatherer.
 * Collects project state information to include in AI prompts.
 */
class IAntigravityContextGatherer
{
public:
	virtual ~IAntigravityContextGatherer() = default;

	/** Build a full project context snapshot */
	virtual FString BuildContextString() = 0;

	/** Get the file tree as a compressed string */
	virtual FString GetFileTreeString() = 0;

	/** Get a summary of all assets in the project */
	virtual FString GetAssetSummaryString() = 0;

	/** Get the current project settings as a JSON string */
	virtual FString GetSettingsSnapshotString() = 0;

	/** Get the class hierarchy for project classes */
	virtual FString GetClassHierarchyString() = 0;

	/** Estimate the token count of the context */
	virtual int32 EstimateTokenCount() const = 0;
};

/**
 * Interface for the safety gate.
 * Determines whether an action can proceed and at what confirmation level.
 */
class IAntigravitySafetyGate
{
public:
	virtual ~IAntigravitySafetyGate() = default;

	/** Evaluate the risk level of an action */
	virtual EAntigravityRiskLevel EvaluateRisk(const FAntigravityAction& Action) const = 0;

	/** Check if an action is allowed based on current safety policy */
	virtual bool IsActionAllowed(const FAntigravityAction& Action, FString& OutReason) const = 0;

	/** Check if a file path is within the allowed write paths */
	virtual bool IsPathAllowed(const FString& FilePath) const = 0;

	/** Validate generated C++ code against denylist patterns */
	virtual bool ValidateGeneratedCode(const FString& Code, TArray<FString>& OutViolations) const = 0;
};

/**
 * Interface for the backup manager.
 * Handles file backups and rollback operations.
 */
class IAntigravityBackupManager
{
public:
	virtual ~IAntigravityBackupManager() = default;

	/** Create a backup of a file before modification */
	virtual FString BackupFile(const FString& FilePath) = 0;

	/** Create backups of multiple files */
	virtual TArray<FString> BackupFiles(const TArray<FString>& FilePaths) = 0;

	/** Restore a file from its backup */
	virtual bool RestoreFile(const FString& BackupPath) = 0;

	/** Restore all files from a specific undo group */
	virtual bool RestoreUndoGroup(const FString& UndoGroupName) = 0;

	/** Get the backup directory path */
	virtual FString GetBackupDirectory() const = 0;

	/** Clean up old backups beyond the rolling window */
	virtual void PruneOldBackups() = 0;
};
