// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * FAntigravityDiagnosticsActions
 *
 * Runtime diagnostics tools:
 *   - read_message_log: Capture the Output Log (errors, warnings, asserts)
 *
 * These are read-only observation tools that let the AI see runtime issues
 * without needing to launch PIE. The Output Log captures Blueprint errors,
 * Accessed None warnings, asset loading failures, and C++ asserts.
 */
class ANTIGRAVITYACTIONS_API FAntigravityDiagnosticsActions : public IAntigravityActionExecutor
{
public:
	FAntigravityDiagnosticsActions();
	virtual ~FAntigravityDiagnosticsActions();

	virtual FName GetActionName() const override;
	virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/**
	 * Read recent entries from the Output Log.
	 *
	 * Captures the last N messages from GLog, optionally filtered by
	 * category (LogBlueprintUserMessages, LogScript, etc.) or severity
	 * (Error, Warning, Display).
	 */
	FAntigravityActionResult ExecuteReadMessageLog(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
	FAntigravityActionResult ExecuteShutdownEditor(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
};
