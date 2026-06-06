// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "AntigravityTypes.generated.h"

// ============================================================================
// Enumerations
// ============================================================================

/** Risk level for AI-driven actions. Determines the confirmation gate behavior. */
UENUM(BlueprintType)
enum class EAntigravityRiskLevel : uint8
{
	/** Auto-approve with log entry. e.g. creating a new empty Blueprint */
	Low			UMETA(DisplayName = "Low"),

	/** Show plan preview, require one-click approval */
	Medium		UMETA(DisplayName = "Medium"),

	/** Show full diff preview, require explicit confirmation */
	High		UMETA(DisplayName = "High"),

	/** Show warning dialog, require typed confirmation phrase */
	Critical	UMETA(DisplayName = "Critical")
};



/** Type of action the AI is requesting */
UENUM(BlueprintType)
enum class EAntigravityActionCategory : uint8
{
	Blueprint		UMETA(DisplayName = "Blueprint"),
	Cpp				UMETA(DisplayName = "C++"),
	Material		UMETA(DisplayName = "Material"),
	Mesh			UMETA(DisplayName = "Mesh"),
	Texture			UMETA(DisplayName = "Texture"),
	Audio			UMETA(DisplayName = "Audio"),
	Animation		UMETA(DisplayName = "Animation"),
	Level			UMETA(DisplayName = "Level"),
	Settings		UMETA(DisplayName = "Settings"),
	Build			UMETA(DisplayName = "Build"),
	Performance		UMETA(DisplayName = "Performance"),
	SourceControl	UMETA(DisplayName = "Source Control"),
	FileSystem		UMETA(DisplayName = "File System"),
	General			UMETA(DisplayName = "General")
};

/** Status of an action execution */
UENUM(BlueprintType)
enum class EAntigravityActionStatus : uint8
{
	Pending			UMETA(DisplayName = "Pending"),
	AwaitingApproval UMETA(DisplayName = "Awaiting Approval"),
	InProgress		UMETA(DisplayName = "In Progress"),
	Succeeded		UMETA(DisplayName = "Succeeded"),
	Failed			UMETA(DisplayName = "Failed"),
	Cancelled		UMETA(DisplayName = "Cancelled"),
	Retrying		UMETA(DisplayName = "Retrying")
};



/** Security mode controlling what actions Antigravity is allowed to perform */
UENUM(BlueprintType)
enum class EAntigravitySecurityMode : uint8
{
	/** Restricted mode: Safe sandboxed asset viewing and editing. Blocks C++, shell, and config changes outside the plugin namespace. */
	Restricted	UMETA(DisplayName = "Restricted"),

	/** Standard mode: Allows C++ code and full asset editing, but blocks compiler execution and scripting. */
	Standard	UMETA(DisplayName = "Standard"),

	/** Full Access mode: Unrestricted privileges. Enables C++ compilation, Live Coding, Python scripting, and editor testing. */
	FullAccess	UMETA(DisplayName = "Full Access")
};





// ============================================================================
// Structures
// ============================================================================



/** Represents a single tool call from the AI */
USTRUCT(BlueprintType)
struct ANTIGRAVITYCORE_API FAntigravityToolCall
{
	GENERATED_BODY()

	/** The unique tool call ID from Antigravity/Gemini */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString ToolCallId;

	/** Name of the tool being called */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString ToolName;

	/** The raw JSON input parameters from the AI */
	TSharedPtr<FJsonObject> InputParams;

	/** Parsed action category */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	EAntigravityActionCategory Category = EAntigravityActionCategory::General;

	FAntigravityToolCall() = default;
};

/** Represents a single discrete action within an action plan */
USTRUCT(BlueprintType)
struct ANTIGRAVITYCORE_API FAntigravityAction
{
	GENERATED_BODY()

	/** Unique ID for this action */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity", Meta = (IgnoreForMemberInitializationTest))
	FGuid ActionId;

	/** Human-readable description of this action */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString Description;

	/** Risk level of this action */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	EAntigravityRiskLevel RiskLevel = EAntigravityRiskLevel::Low;

	/** Current execution status */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	EAntigravityActionStatus Status = EAntigravityActionStatus::Pending;

	/** Category of the action */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	EAntigravityActionCategory Category = EAntigravityActionCategory::General;

	/** The tool call that initiated this action */
	FAntigravityToolCall ToolCall;

	/** List of file paths that will be affected */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> AffectedPaths;

	/** List of assets that will be affected */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> AffectedAssets;

	FAntigravityAction()
		: ActionId(FGuid::NewGuid())
	{
	}
};

/** Collection of ordered actions forming a plan */
USTRUCT(BlueprintType)
struct ANTIGRAVITYCORE_API FAntigravityActionPlan
{
	GENERATED_BODY()

	/** Unique ID for this plan */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity", Meta = (IgnoreForMemberInitializationTest))
	FGuid PlanId;

	/** Human-readable summary of the full plan */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString Summary;

	/** Ordered list of actions in this plan */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FAntigravityAction> Actions;

	/** Maximum risk level across all actions */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	EAntigravityRiskLevel MaxRiskLevel = EAntigravityRiskLevel::Low;

	/** Undo group name for rolling back the entire plan */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString UndoGroupName;

	FAntigravityActionPlan()
		: PlanId(FGuid::NewGuid())
	{
	}
};

/** Result of executing a single action */
USTRUCT(BlueprintType)
struct ANTIGRAVITYCORE_API FAntigravityActionResult
{
	GENERATED_BODY()

	/** Whether the action succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	bool bSuccess = false;

	/** The action that was executed */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity", Meta = (IgnoreForMemberInitializationTest))
	FGuid ActionId;

	/** Human-readable result message */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString ResultMessage;

	/** Error messages if the action failed */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> Errors;

	/** Warning messages */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> Warnings;

	/** Paths of files that were created or modified */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> ModifiedPaths;

	/** Paths of assets that were created or modified */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> ModifiedAssets;

	/** Paths of backup files that were created */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> BackupPaths;

	/** Time taken to execute in seconds */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	float ExecutionTimeSeconds = 0.0f;
};



/** File entry in the project context tree */
USTRUCT(BlueprintType)
struct ANTIGRAVITYCORE_API FAntigravityFileEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString Path;

	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString Extension;

	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	bool bIsDirectory = false;

	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	int64 FileSize = 0;
};

/** Asset entry in the project context */
USTRUCT(BlueprintType)
struct ANTIGRAVITYCORE_API FAntigravityAssetEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString AssetName;

	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString AssetClass;

	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> Dependencies;
};

/** Record of a single action execution for the audit journal */
USTRUCT(BlueprintType)
struct ANTIGRAVITYCORE_API FAntigravityActionExecutionRecord
{
	GENERATED_BODY()

	/** Unique ID for this execution record */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity", Meta = (IgnoreForMemberInitializationTest))
	FGuid RecordId;

	/** Name of the tool that was executed */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString ToolName;

	/** The unique tool call ID from Antigravity/Gemini */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString ToolCallId;

	/** Raw JSON input parameters (serialized) */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString InputJson;

	/** Result message */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString ResultMessage;

	/** Whether the action succeeded */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	bool bSuccess = false;

	/** Whether the tool result was an error (for is_error in tool_result) */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	bool bIsError = false;

	/** List of files that were modified */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> ModifiedFiles;

	/** List of assets that were modified */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> ModifiedAssets;

	/** Backup paths for rollback */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> BackupPaths;

	/** SHA-1 hash of the primary affected file/asset BEFORE execution */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString PreStateHash;

	/** SHA-1 hash of the primary affected file/asset AFTER execution */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString PostStateHash;

	/** Timestamp of execution */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FDateTime Timestamp;

	/** Execution time in seconds */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	float ExecutionTimeSeconds = 0.0f;

	FAntigravityActionExecutionRecord()
		: RecordId(FGuid::NewGuid())
		, Timestamp(FDateTime::UtcNow())
	{
	}
};

/** Current editor context snapshot -- what the user is looking at */
USTRUCT(BlueprintType)
struct ANTIGRAVITYCORE_API FAntigravityEditorContext
{
	GENERATED_BODY()

	/** Name of the currently active level/map */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString ActiveLevelName;

	/** Summary of selected actors in the viewport */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString SelectedActorsSummary;

	/** Summary of selected assets in the content browser */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString SelectedAssetsSummary;

	/** List of currently open editor tabs/windows */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> OpenEditors;

	/** Current viewport camera location as string */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString ViewportCameraInfo;

	/** Number of actors in the current level */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	int32 ActorCount = 0;

	/** Build as a context string for the system prompt */
	FString ToContextString() const
	{
		FString Ctx = TEXT("=== Editor Context ===\n");
		Ctx += FString::Printf(TEXT("Active Level: %s\n"), *ActiveLevelName);
		Ctx += FString::Printf(TEXT("Actor Count: %d\n"), ActorCount);
		if (!SelectedActorsSummary.IsEmpty())
			Ctx += FString::Printf(TEXT("Selected Actors: %s\n"), *SelectedActorsSummary);
		if (!SelectedAssetsSummary.IsEmpty())
			Ctx += FString::Printf(TEXT("Selected Assets: %s\n"), *SelectedAssetsSummary);
		if (OpenEditors.Num() > 0)
		{
			Ctx += TEXT("Open Editors: ");
			Ctx += FString::Join(OpenEditors, TEXT(", "));
			Ctx += TEXT("\n");
		}
		return Ctx;
	}
};


