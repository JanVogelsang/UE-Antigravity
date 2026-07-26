// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

class UNiagaraSystem;
class UNiagaraGraph;

class AGENTFRAMEWORKACTIONS_API FAgentFrameworkNiagaraActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkNiagaraActions();
	virtual ~FAgentFrameworkNiagaraActions();

	// IAgentFrameworkActionExecutor interface
	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	// Core Tool Execution Handlers
	FAgentFrameworkActionResult ExecuteCreateSystem(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteAddEmitter(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteAddModule(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteSetModulePin(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteCompileSystem(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteCaptureIsolated(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteSetNiagaraParameter(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	// Helper Graph-Editing and Compile Methods
	UNiagaraGraph* FindGraphForPhase(UNiagaraSystem* System, const FString& EmitterName, const FString& PhaseStr, FString& OutError) const;
	bool WaitAndReportCompile(UNiagaraSystem* System, FAgentFrameworkActionResult& Result) const;

	// Audio feedback helper
	void PlaySuccessSound();
};

