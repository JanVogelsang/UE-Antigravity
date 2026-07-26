// Copyright 2026 AgentFramework. All Rights Reserved.

#include "AIAssistant/AgentFrameworkAIAssistantActions.h"
#include "AIAssistant/AIAssistantBridge.h"
#include "AgentFrameworkActionUtils.h"
#include "Modules/ModuleManager.h"

TStrongObjectPtr<UAIAssistantBridge> FAgentFrameworkAIAssistantActions::BridgeInstance = nullptr;

FAgentFrameworkAIAssistantActions::FAgentFrameworkAIAssistantActions()
{
}

FAgentFrameworkAIAssistantActions::~FAgentFrameworkAIAssistantActions()
{
	// Clean up bridge on shutdown
	if (BridgeInstance.IsValid())
	{
		BridgeInstance.Reset();
	}
}

FName FAgentFrameworkAIAssistantActions::GetActionName() const
{
	return FName(TEXT("AIAssistant"));
}

TArray<FString> FAgentFrameworkAIAssistantActions::GetSupportedToolNames() const
{
	return { TEXT("query_epic_assistant") };
}

bool FAgentFrameworkAIAssistantActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString Prompt;
	return UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("prompt"), Prompt, OutErrors, true);
}

UAIAssistantBridge* FAgentFrameworkAIAssistantActions::GetBridgeInstance()
{
	if (!BridgeInstance.IsValid())
	{
		UAIAssistantBridge* NewBridge = NewObject<UAIAssistantBridge>();
		if (IsValid(NewBridge))
		{
			BridgeInstance = TStrongObjectPtr<UAIAssistantBridge>(NewBridge);
		}
	}
	return BridgeInstance.IsValid() ? BridgeInstance.Get() : nullptr;
}

FAgentFrameworkActionResult FAgentFrameworkAIAssistantActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	if (!FModuleManager::Get().IsModuleLoaded("AIAssistant"))
	{
		Result.Errors.Add(TEXT("The Epic AIAssistant plugin is not enabled or loaded."));
		return Result;
	}

	FString Prompt;
	TArray<FString> Errors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("prompt"), Prompt, Errors, false))
	{
		Result.Errors.Append(Errors);
		return Result;
	}

	UAIAssistantBridge* Bridge = GetBridgeInstance();
	if (!IsValid(Bridge))
	{
		Result.Errors.Add(TEXT("Failed to create AIAssistant bridge instance."));
		return Result;
	}

	if (!Bridge->InitializeBridge())
	{
		Result.Errors.Add(TEXT("Failed to locate or initialize AIAssistant web browser. Make sure the AI Assistant tab is open."));
		return Result;
	}

	Bridge->SetActiveQueryPrompt(Prompt);

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("PENDING");
	return Result;
}

