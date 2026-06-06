// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * FAntigravityPIEActions
 *
 * Play-In-Editor automation tools:
 *   - start_pie_session: Launch PIE for runtime testing
 *   - simulate_input: Inject key presses during PIE
 *   - stop_pie_session: End the current PIE session
 *
 * Combined with read_message_log (from DiagnosticsActions), the AI can:
 *   1. Build a Blueprint
 *   2. Start PIE
 *   3. Simulate player input (move forward, press interact)
 *   4. Read the Output Log for Accessed None errors
 *   5. Stop PIE and fix the bugs
 *
 * SAFETY:
 *   - PIE is inherently risky (editor crash potential), so rated High risk
 *   - Requires Full Access security mode
 *   - Input simulation is limited to keyboard/gamepad (no mouse movement)
 *   - PIE sessions auto-stop after a configurable timeout
 */
class ANTIGRAVITYACTIONS_API FAntigravityPIEActions : public IAntigravityActionExecutor
{
public:
	FAntigravityPIEActions();
	virtual ~FAntigravityPIEActions();

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
	/** Launch Play-In-Editor. */
	FAntigravityActionResult ExecuteStartPIE(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Inject keyboard input during a running PIE session. */
	FAntigravityActionResult ExecuteSimulateInput(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Stop the current PIE session. */
	FAntigravityActionResult ExecuteStopPIE(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Whether PIE is currently running. */
	static bool IsPIERunning();
};
