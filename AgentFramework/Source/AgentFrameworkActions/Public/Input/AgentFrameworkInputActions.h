// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

/**
 * Enhanced Input actions for AgentFramework.
 *
 * Provides programmatic creation and modification of Enhanced Input assets:
 * - create_input_action: Create a UInputAction data asset (IA_*)
 * - create_input_mapping_context: Create a UInputMappingContext data asset (IMC_*)
 * - add_input_mapping: Add a keyâ†’action binding to an existing IMC (with optional modifiers/triggers)
 *
 * These tools eliminate the need for manual editor interaction when configuring
 * the Enhanced Input System, upholding the zero-manual-steps protocol.
 */
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkInputActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkInputActions();
	virtual ~FAgentFrameworkInputActions();

	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	FAgentFrameworkActionResult ExecuteCreateInputAction(const TSharedRef<FJsonObject>& Params);
	FAgentFrameworkActionResult ExecuteCreateInputMappingContext(const TSharedRef<FJsonObject>& Params);
	FAgentFrameworkActionResult ExecuteAddInputMapping(const TSharedRef<FJsonObject>& Params);
};
