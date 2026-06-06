// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * Enhanced Input actions for Antigravity.
 *
 * Provides programmatic creation and modification of Enhanced Input assets:
 * - create_input_action: Create a UInputAction data asset (IA_*)
 * - create_input_mapping_context: Create a UInputMappingContext data asset (IMC_*)
 * - add_input_mapping: Add a keyâ†’action binding to an existing IMC (with optional modifiers/triggers)
 *
 * These tools eliminate the need for manual editor interaction when configuring
 * the Enhanced Input System, upholding the zero-manual-steps protocol.
 */
class ANTIGRAVITYACTIONS_API FAntigravityInputActions : public IAntigravityActionExecutor
{
public:
	FAntigravityInputActions();
	virtual ~FAntigravityInputActions();

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
	FAntigravityActionResult ExecuteCreateInputAction(const TSharedRef<FJsonObject>& Params);
	FAntigravityActionResult ExecuteCreateInputMappingContext(const TSharedRef<FJsonObject>& Params);
	FAntigravityActionResult ExecuteAddInputMapping(const TSharedRef<FJsonObject>& Params);
};
