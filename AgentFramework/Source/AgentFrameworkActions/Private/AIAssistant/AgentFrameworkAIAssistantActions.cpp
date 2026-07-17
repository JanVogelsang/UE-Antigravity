// Copyright 2026 AgentFramework. All Rights Reserved.

#include "AIAssistant/AgentFrameworkAIAssistantActions.h"
#include "AIAssistant/AIAssistantBridge.h"
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
	if (!Params->TryGetStringField(TEXT("prompt"), Prompt) || Prompt.IsEmpty())
	{
		OutErrors.Add(TEXT("Parameter 'prompt' is required and must not be empty."));
		return false;
	}
	return true;
}

UAIAssistantBridge* FAgentFrameworkAIAssistantActions::GetBridgeInstance()
{
	if (!BridgeInstance.IsValid())
	{
		UAIAssistantBridge* NewBridge = NewObject<UAIAssistantBridge>();
		BridgeInstance = TStrongObjectPtr<UAIAssistantBridge>(NewBridge);
	}
	return BridgeInstance.Get();
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
	Params->TryGetStringField(TEXT("prompt"), Prompt);

	UAIAssistantBridge* Bridge = GetBridgeInstance();
	if (!Bridge)
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
