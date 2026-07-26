// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

class AGENTFRAMEWORKACTIONS_API FAgentFrameworkCppActions : public IAgentFrameworkActionExecutor
{
public:
    FAgentFrameworkCppActions();
    virtual ~FAgentFrameworkCppActions();

    virtual FName GetActionName() const override;
    virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
    virtual TArray<FString> GetSupportedToolNames() const override;
    virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
    FAgentFrameworkActionResult ExecuteMacroCreateCppClass(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
    FAgentFrameworkActionResult ExecuteCreateCppClass(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
    FAgentFrameworkActionResult ExecuteModifyCppFile(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
    FAgentFrameworkActionResult ExecuteTriggerCompile(FAgentFrameworkActionResult& Result);
    FAgentFrameworkActionResult ExecuteRegenerateProjectFiles(FAgentFrameworkActionResult& Result);
    FAgentFrameworkActionResult ExecuteGetCppReflectionInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

    /** Validate generated code for dangerous patterns */
    bool ValidateCodeSafety(const FString& Code, TArray<FString>& OutViolations) const;

    /** Write a file to disk with backup */
    bool WriteFileWithBackup(const FString& FilePath, const FString& Content);

    /** Play success notification sound */
    void PlaySuccessSound();
};

