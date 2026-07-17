// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SWebBrowser.h"
#include "AIAssistantBridge.generated.h"

DECLARE_DELEGATE_TwoParams(FAIAssistantResponseDelegate, const FString&, bool);

UCLASS(BlueprintType)
class AGENTFRAMEWORKACTIONS_API UAIAssistantBridge : public UObject
{
	GENERATED_BODY()

public:
	UAIAssistantBridge();

	/** Binds this object to the active AIAssistant web browser */
	bool InitializeBridge();

	/** Sends a prompt to the assistant and registers a callback for the response */
	bool SendQuery(const FString& InPrompt, FAIAssistantResponseDelegate InCallback);

	/** UFUNCTION that will be invoked from JS context */
	UFUNCTION(BlueprintCallable, Category = "AgentFramework")
	void OnResponseReceived(const FString& Response, bool bSuccess);

	void SetActiveQueryPrompt(const FString& InPrompt) { ActiveQueryPrompt = InPrompt; }
	FString GetActiveQueryPrompt() const { return ActiveQueryPrompt; }

private:
	TSharedPtr<SWebBrowser> FindWebBrowserWidget();
	TSharedPtr<SWidget> FindWidgetOfClass(TSharedPtr<SWidget> StartWidget, const FName& ClassName);

	FAIAssistantResponseDelegate CurrentCallback;
	TWeakPtr<SWebBrowser> BoundWebBrowser;
	FString ActiveQueryPrompt;
};
