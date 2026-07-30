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
    /** @param bWaitForCompletion Block until Live Coding reports a result instead of returning as soon as the compile is queued. */
    FAgentFrameworkActionResult ExecuteTriggerCompile(FAgentFrameworkActionResult& Result, bool bWaitForCompletion = true);
    FAgentFrameworkActionResult ExecuteRegenerateProjectFiles(FAgentFrameworkActionResult& Result);
    FAgentFrameworkActionResult ExecuteGetCppReflectionInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

    /** Validate generated code for dangerous patterns */
    bool ValidateCodeSafety(const FString& Code, TArray<FString>& OutViolations) const;

    /** Write a file to disk with backup */
    /**
     * Backs up and writes a source file, rejecting any path outside the project's own source
     * trees (see IsWritableSourcePath). Every write in this executor goes through here.
     *
     * @param OutError Set to a caller-reportable reason when the write is refused or fails.
     */
    bool WriteFileWithBackup(const FString& FilePath, const FString& Content, FString& OutError);

    /** Play success notification sound */
    void PlaySuccessSound();
};

