// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityTypes.h"
#include "AntigravityProjectContext.generated.h"

/**
 * Serializable snapshot of the current project state.
 * Sent to the AI as context with every request.
 */
USTRUCT(BlueprintType)
struct ANTIGRAVITYCORE_API FAntigravityProjectContext
{
	GENERATED_BODY()

	/** Name of the Unreal project */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString ProjectName;

	/** Engine version string (e.g. "5.5.0") */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString EngineVersion;

	/** Absolute path to the project root */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString ProjectRootPath;

	/** Content directory tree entries */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FAntigravityFileEntry> ContentTree;

	/** Source directory tree entries */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FAntigravityFileEntry> SourceTree;

	/** Config directory tree entries */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FAntigravityFileEntry> ConfigTree;

	/** All registered assets in the project */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FAntigravityAssetEntry> Assets;

	/** Summary counts by asset type */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TMap<FString, int32> AssetCountsByClass;

	/** Currently loaded/open level path */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FString CurrentLevelPath;

	/** List of modules in the project (from .uproject) */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> ProjectModules;

	/** List of enabled plugins */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> EnabledPlugins;

	/** Target platform(s) configured */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	TArray<FString> TargetPlatforms;

	/** Timestamp of when this context was captured */
	UPROPERTY(BlueprintReadOnly, Category = "Antigravity")
	FDateTime CaptureTimestamp;

	FAntigravityProjectContext()
		: CaptureTimestamp(FDateTime::UtcNow())
	{
	}

	/** Serialize the entire context to a compressed string for the AI system prompt */
	FString ToContextString() const;

	/** Estimate the approximate token count of the serialized context */
	int32 EstimateTokenCount() const;
};
