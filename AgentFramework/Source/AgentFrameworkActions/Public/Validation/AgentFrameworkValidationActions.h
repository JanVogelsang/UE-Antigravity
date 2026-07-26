// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

/**
 * FAgentFrameworkValidationActions
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
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkValidationActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkValidationActions();
	virtual ~FAgentFrameworkValidationActions();

	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/**
	 * Run UEditorValidatorSubsystem on specified assets or all project assets.
	 *
	 * @param Params  JSON with optional "asset_paths" (array of content paths) or
	 *                "validate_all" (bool) to validate the entire project
	 */
	FAgentFrameworkActionResult ExecuteValidateAssets(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/**
	 * Run Unreal Automation Tests matching a filter pattern.
	 *
	 * @param Params  JSON with "test_filter" (string pattern) and optional
	 *                "test_flags" (string: "Smoke", "Product", "Stress", etc.)
	 */
	FAgentFrameworkActionResult ExecuteRunAutomationTests(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/**
	 * Validate asset naming conventions against standard Unreal Engine prefixes.
	 *
	 * @param Params  JSON with optional "path" or "search_path" (content path, default "/Game/")
	 *                and optional "asset_paths" (array of asset paths).
	 */
	FAgentFrameworkActionResult ExecuteValidateNamingConventions(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/**
	 * Scan specified content path for ObjectRedirectors.
	 *
	 * @param Params  JSON with optional "path" or "search_path" (content path, default "/Game/")
	 *                and optional "fix_redirectors" (bool).
	 */
	FAgentFrameworkActionResult ExecuteValidateRedirectors(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/**
	 * Validate a level/map asset or current editor world for common map issues.
	 *
	 * @param Params  JSON with optional "map_path" (content path to level asset).
	 */
	FAgentFrameworkActionResult ExecuteValidateMap(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
};
