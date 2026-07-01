// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * UE-Specific Context Executor.
 * 
 * Provides on-demand tools for Antigravity (Gemini) to explore the project.
 * 
 * - search_assets: Search the asset registry by class, name, or path
 * 
 * These tools are always available regardless of security mode (read-only).
 */
class ANTIGRAVITYACTIONS_API FAntigravityContextActions : public IAntigravityActionExecutor
{
public:
    FAntigravityContextActions();
    virtual ~FAntigravityContextActions();

    virtual FName GetActionName() const override;
    virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
    virtual TArray<FString> GetSupportedToolNames() const override;
    virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
    FAntigravityActionResult ExecuteSearchAssets(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
};
