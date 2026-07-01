// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "AntigravityTypes.generated.h"

// ============================================================================
// Enumerations
// ============================================================================





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



	FAntigravityToolCall() = default;
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


