// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AntigravityTypes.h"
#include "AntigravitySettings.generated.h"

/**
 * Antigravity plugin settings.
 *
 * Access at runtime via: GetDefault<UAntigravityDeveloperSettings>()
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FAntigravitySettingsChangedDelegate, FName /*PropertyName*/);

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Antigravity"))
class ANTIGRAVITYCORE_API UAntigravityDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAntigravityDeveloperSettings();

	static FAntigravitySettingsChangedDelegate OnSettingsChanged;

	// ============================================================================
	// Safety & Security Settings
	// ============================================================================

	/** Security mode controlling what capabilities Antigravity is allowed to use.
	 *  Defaults to Sandbox for safety. Developer mode requires explicit opt-in. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Safety|Security",
		meta = (DisplayName = "Security Mode",
		ToolTip = "Sandbox: No C++/builds/shell. Advanced: Full asset editing. Developer: Full power including C++ and builds."))
	EAntigravitySecurityMode SecurityMode;

	// ============================================================================
	// Protected Files (read-only for AI)
	// ============================================================================

	/** Additional file path patterns that the AI may read but must never modify.
	 *  Uses glob syntax: *.uplugin, Config/Default*.ini, Source/[Project].Build.cs
	 *
	 *  The following paths are always protected by default regardless of this list:
	 *  *.uplugin, *.uproject, *.Build.cs, *.Target.cs, Config/DefaultEngine.ini,
	 *  Config/DefaultEditor.ini, Config/DefaultGame.ini, .gitignore, .antigravityignore
	 *
	 *  Write attempts to protected paths always return EAntigravityRiskLevel::Critical. */
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
	// Skills Settings
	// ============================================================================

	/** Enable AI self-improvement loop for skills.
	 *  When enabled, Antigravity tracks active skills, detects failures or warnings,
	 *  and appends self-improvement alerts to the response. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Skills",
		meta = (DisplayName = "Enable Skills Self-Improvement Loop",
		ToolTip = "Analyze tool failures and compilation errors while a skill is active and notify the user to help improve the skill definitions."))
	bool bEnableSkillsSelfImprovementLoop;

	// ============================================================================
	// Utility
	// ============================================================================

	static const UAntigravityDeveloperSettings* Get();
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
