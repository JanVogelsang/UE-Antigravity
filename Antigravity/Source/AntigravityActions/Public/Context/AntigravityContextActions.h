// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * Context-as-tools executor.
 * 
 * Instead of front-loading the entire project context into the system prompt
 * (which can blow past token limits on large projects), this executor provides
 * on-demand tools that Claude calls to explore the project:
 * 
 * - list_directory: List files/folders in a project-relative path
 * - search_assets: Search the asset registry by class, name, or path
 * - read_file_snippet: Read a section of a source file (with line range)
 * 
 * These tools are always available regardless of security mode (read-only).
 */
class ANTIGRAVITYACTIONS_API FAntigravityContextActions : public IAntigravityActionExecutor
{
public:
    FAntigravityContextActions();
    virtual ~FAntigravityContextActions();

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
    FAntigravityActionResult ExecuteListDirectory(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
    FAntigravityActionResult ExecuteSearchAssets(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
    FAntigravityActionResult ExecuteReadFileSnippet(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

    /** Validate that a path doesn't escape the project directory */
    bool IsPathSafe(const FString& RelativePath) const;
};
