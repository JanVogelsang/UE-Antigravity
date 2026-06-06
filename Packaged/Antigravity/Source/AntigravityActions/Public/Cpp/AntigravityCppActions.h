// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

class ANTIGRAVITYACTIONS_API FAntigravityCppActions : public IAntigravityActionExecutor
{
public:
    FAntigravityCppActions();
    virtual ~FAntigravityCppActions();

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
    FAntigravityActionResult ExecuteMacroCreateCppClass(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
    FAntigravityActionResult ExecuteCreateCppClass(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
    FAntigravityActionResult ExecuteModifyCppFile(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
    FAntigravityActionResult ExecuteTriggerCompile(FAntigravityActionResult& Result);
    FAntigravityActionResult ExecuteRegenerateProjectFiles(FAntigravityActionResult& Result);

    /** Validate generated code for dangerous patterns */
    bool ValidateCodeSafety(const FString& Code, TArray<FString>& OutViolations) const;

    /** Write a file to disk with backup */
    bool WriteFileWithBackup(const FString& FilePath, const FString& Content);
};
