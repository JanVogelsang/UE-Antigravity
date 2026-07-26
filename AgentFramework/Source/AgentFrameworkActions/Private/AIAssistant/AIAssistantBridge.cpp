// Copyright 2026 AgentFramework. All Rights Reserved.

#include "AIAssistant/AIAssistantBridge.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "SWebBrowser.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

UAIAssistantBridge::UAIAssistantBridge()
	: QueryCompletedSound(nullptr)
{
}

TSharedPtr<SWidget> UAIAssistantBridge::FindWidgetOfClass(TSharedPtr<SWidget> StartWidget, const FName& ClassName)
{
	if (!StartWidget.IsValid())
	{
		return nullptr;
	}

	if (StartWidget->GetType() == ClassName || StartWidget->GetTypeAsString() == ClassName.ToString())
	{
		return StartWidget;
	}

	FChildren* Children = StartWidget->GetChildren();
	if (Children)
	{
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			TSharedPtr<SWidget> Found = FindWidgetOfClass(Children->GetChildAt(i), ClassName);
			if (Found.IsValid())
			{
				return Found;
			}
		}
	}

	return nullptr;
}

TSharedPtr<SWebBrowser> UAIAssistantBridge::FindWebBrowserWidget()
{
	if (!FModuleManager::Get().IsModuleLoaded("AIAssistant"))
	{
		return nullptr;
	}

	TSharedPtr<SDockTab> AIAssistantTab = FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId(TEXT("AIAssistant")));
	if (!AIAssistantTab.IsValid())
	{
		// Try to invoke it first
		AIAssistantTab = FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("AIAssistant")));
	}

	if (AIAssistantTab.IsValid())
	{
		TSharedPtr<SWidget> BrowserWidget = FindWidgetOfClass(AIAssistantTab, TEXT("SWebBrowser"));
		if (BrowserWidget.IsValid())
		{
			return StaticCastSharedPtr<SWebBrowser>(BrowserWidget);
		}
	}

	return nullptr;
}

bool UAIAssistantBridge::InitializeBridge()
{
	if (!IsValid(this))
	{
		return false;
	}
	TSharedPtr<SWebBrowser> WebBrowser = FindWebBrowserWidget();
	if (!WebBrowser.IsValid())
	{
		return false;
	}

	BoundWebBrowser = WebBrowser;

	// Bind this UObject to the CEF web page
	// Lowercase registration name since UE's WebBrowser binds names in lowercase for Javascript
	WebBrowser->BindUObject(TEXT("ouragentbridge"), this, true);
	return true;
}

bool UAIAssistantBridge::SendQuery(const FString& InPrompt, FAIAssistantResponseDelegate InCallback)
{
	if (!IsValid(this))
	{
		return false;
	}
	TSharedPtr<SWebBrowser> WebBrowser = BoundWebBrowser.Pin();
	if (!WebBrowser.IsValid())
	{
		// Attempt re-initialization
		if (InitializeBridge())
		{
			WebBrowser = BoundWebBrowser.Pin();
		}
	}

	if (!WebBrowser.IsValid())
	{
		return false;
	}

	CurrentCallback = InCallback;

	// Escape prompt string for JS
	FString EscapedPrompt = InPrompt;
	EscapedPrompt = EscapedPrompt.Replace(TEXT("\\"), TEXT("\\\\"));
	EscapedPrompt = EscapedPrompt.Replace(TEXT("'"), TEXT("\\'"));
	EscapedPrompt = EscapedPrompt.Replace(TEXT("\n"), TEXT("\\n"));
	EscapedPrompt = EscapedPrompt.Replace(TEXT("\r"), TEXT("\\r"));

	// Inject the JS script to register the update listener and add message to conversation.
	FString Script = FString::Printf(TEXT(R"js(
(function() {
  if (typeof window.eda === 'undefined') {
    return 'EDA_NOT_AVAILABLE';
  }
  if (!window.ourAgentBridgeListenerSetup) {
    window.ourAgentBridgeListenerSetup = true;
    window.eda.registerOnConversationUpdate((event) => {
      if (event.updateType === 'complete') {
        window.eda.getConversation(event.conversationId).then((conversation) => {
          if (conversation && conversation.messages && conversation.messages.length > 0) {
            let lastAgentText = "";
            for (let i = conversation.messages.length - 1; i >= 0; i--) {
              let msg = conversation.messages[i];
              if (msg.messageRole === 'agent') {
                if (msg.messageContent) {
                  for (let content of msg.messageContent) {
                    if (content.contentType === 'text' && content.content && content.content.text) {
                      lastAgentText = content.content.text;
                      break;
                    }
                  }
                }
                if (lastAgentText) break;
              }
            }
            if (window.ue && window.ue.ouragentbridge) {
              window.ue.ouragentbridge.onresponsereceived(lastAgentText, true);
            }
          }
        }).catch((err) => {
          if (window.ue && window.ue.ouragentbridge) {
            window.ue.ouragentbridge.onresponsereceived(err.toString(), false);
          }
        });
      }
    });
  }
  window.eda.addMessageToConversation({
    message: {
      messageRole: 'user',
      messageContent: [{
        contentType: 'text',
        content: { text: '%s' },
        visibleToUser: true
      }]
    }
  });
  return 'SUCCESS';
})();
)js"), *EscapedPrompt);

	WebBrowser->ExecuteJavascript(Script);
	return true;
}

void UAIAssistantBridge::OnResponseReceived(const FString& Response, bool bSuccess)
{
	if (!IsValid(this))
	{
		return;
	}

	FAIAssistantResponseDelegate Callback = CurrentCallback;
	CurrentCallback.Unbind();

	if (Callback.IsBound())
	{
		Callback.Execute(Response, bSuccess);
	}

	if (OnQueryCompleted.IsBound())
	{
		OnQueryCompleted.Broadcast(Response, bSuccess);
	}

	if (OnQueryCompletedDynamic.IsBound())
	{
		OnQueryCompletedDynamic.Broadcast(Response, bSuccess);
	}

#if WITH_EDITOR
	if (IsValid(QueryCompletedSound) && GEditor)
	{
		GEditor->PlayEditorSound(QueryCompletedSound);
	}
#endif
}
