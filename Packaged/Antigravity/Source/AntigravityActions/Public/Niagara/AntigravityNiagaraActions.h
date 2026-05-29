// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

class UNiagaraSystem;
class UNiagaraEmitter;
class UNiagaraGraph;

class ANTIGRAVITYACTIONS_API FAntigravityNiagaraActions : public IAntigravityActionExecutor
{
public:
	FAntigravityNiagaraActions();
	virtual ~FAntigravityNiagaraActions();

	// IAntigravityActionExecutor interface
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
	// Core Tool Execution Handlers
	FAntigravityActionResult ExecuteCreateSystem(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
	FAntigravityActionResult ExecuteAddEmitter(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
	FAntigravityActionResult ExecuteAddModule(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
	FAntigravityActionResult ExecuteSetModulePin(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
	FAntigravityActionResult ExecuteCompileSystem(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
	FAntigravityActionResult ExecuteCaptureIsolated(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	// Helper Graph-Editing and Compile Methods
	UNiagaraGraph* FindGraphForPhase(UNiagaraSystem* System, const FString& EmitterName, const FString& PhaseStr, FString& OutError) const;
	bool WaitAndReportCompile(UNiagaraSystem* System, FAntigravityActionResult& Result) const;
};
