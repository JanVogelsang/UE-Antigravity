// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AgentFrameworkTypes.h"
#include "AgentFrameworkSettings.generated.h"

/**
 * AgentFramework plugin settings.
 *
 * Access at runtime via: GetDefault<UAgentFrameworkDeveloperSettings>()
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FAgentFrameworkSettingsChangedDelegate, FName /*PropertyName*/);

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "AgentFramework"))
class AGENTFRAMEWORKCORE_API UAgentFrameworkDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAgentFrameworkDeveloperSettings();

	static FAgentFrameworkSettingsChangedDelegate OnSettingsChanged;

	// ============================================================================
	// Safety & Security Settings
	// ============================================================================

	/** Security mode controlling what capabilities AgentFramework is allowed to use.
	 *  Defaults to Full Access (Unrestricted) mode. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Safety|Security",
		meta = (DisplayName = "Security Mode",
		ToolTip = "Restricted: Safe sandboxed asset viewing/editing. Standard: Full source code and asset editing. Full Access: Unrestricted power (compiling, Live Coding, python, and testing)."))
	EAgentFrameworkSecurityMode SecurityMode;

	// ============================================================================
	// Protected Files (read-only for AI)
	// ============================================================================

	/** Additional file path patterns that the AI may read but must never modify.
	 *  Uses glob syntax: *.uplugin, Config/Default*.ini, Source/[Project].Build.cs
	 *
	 *  The following paths are always protected by default regardless of this list:
	 *  *.uplugin, *.uproject, *.Build.cs, *.Target.cs, Config/DefaultEngine.ini,
	 *  Config/DefaultEditor.ini, Config/DefaultGame.ini, .gitignore, .agentframeworkignore
	 *
	 *  Write attempts to protected paths always return EAgentFrameworkRiskLevel::Critical. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Safety|Protected Files",
		meta = (DisplayName = "Additional Protected File Patterns",
		ToolTip = "Glob patterns for files the AI may read but must never write. Defaults cover *.uplugin, *.uproject, *.Build.cs, core Config/*.ini files."))
	TArray<FString> AdditionalProtectedPaths;

	/** Override the default protected paths list with only the paths in AdditionalProtectedPaths.
	 *  When false (default), AdditionalProtectedPaths supplements the built-in defaults.
	 *  When true, ONLY AdditionalProtectedPaths is used — use with caution. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Safety|Protected Files",
		meta = (DisplayName = "Override Default Protected Paths",
		ToolTip = "When enabled, replaces the built-in protected file list with only your custom patterns."))
	bool bOverrideDefaultProtectedPaths = false;



	// ============================================================================
	// Utility
	// ============================================================================

	static const UAgentFrameworkDeveloperSettings* Get();
	bool IsIniSectionAllowed(const FString& Section) const;

	// UDeveloperSettings interface
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
