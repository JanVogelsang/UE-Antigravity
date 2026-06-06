// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * FAntigravityValidationActions
 *
 * Asset validation and testing tools:
 *   - validate_assets: Run UEditorValidatorSubsystem on specified assets
 *   - run_automation_tests: Execute Unreal Automation Tests by name/group
 *
 * These tools form the "verification ladder" that the AI uses to confirm
 * its work is correct beyond just "it compiles":
 *   Step 1: Compile Blueprint / Live Coding (existing tools)
 *   Step 2: validate_assets â€” official UE data validation
 *   Step 3: run_automation_tests â€” project + engine tests
 *   Step 4: Performance profiling (existing tools)
 *
 * Data Validation runs registered UEditorValidator instances, which check:
 *   - Missing/broken asset references
 *   - Invalid property values
 *   - Custom project validators (if registered)
 *   - Blueprint compilation status
 *
 * Automation Tests run the UE test framework, which can verify:
 *   - Functional tests (spawn actors, check behavior)
 *   - Unit tests (C++ logic)
 *   - Smoke tests (load maps, check for errors)
 */
class ANTIGRAVITYACTIONS_API FAntigravityValidationActions : public IAntigravityActionExecutor
{
public:
	FAntigravityValidationActions();
	virtual ~FAntigravityValidationActions();

	virtual FName GetActionName() const override;
	virtual FText GetDisplayName() const override;
	virtual EAntigravityActionCategory GetCategory() const override;
	virtual EAntigravityRiskLevel GetDefaultRiskLevel() const override;
	virtual FAntigravityActionPlan PreviewAction(const TSharedRef<FJsonObject>& Params) override;
	virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual bool CanUndo() const override;
	virtual bool UndoAction() override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/**
	 * Run UEditorValidatorSubsystem on specified assets or all project assets.
	 *
	 * @param Params  JSON with optional "asset_paths" (array of content paths) or
	 *                "validate_all" (bool) to validate the entire project
	 */
	FAntigravityActionResult ExecuteValidateAssets(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/**
	 * Run Unreal Automation Tests matching a filter pattern.
	 *
	 * @param Params  JSON with "test_filter" (string pattern) and optional
	 *                "test_flags" (string: "Smoke", "Product", "Stress", etc.)
	 */
	FAntigravityActionResult ExecuteRunAutomationTests(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
};
