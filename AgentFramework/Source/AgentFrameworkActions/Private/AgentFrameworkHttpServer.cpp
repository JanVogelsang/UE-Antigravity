// Copyright 2026 AgentFramework. All Rights Reserved.

#include "AgentFrameworkHttpServer.h"
#include "HttpServerModule.h"
#include "HttpPath.h"
#include "IHttpRouter.h"
#include "HttpServerResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Async/Async.h"
#include "HAL/PlatformFileManager.h"
#include "AgentFrameworkSettings.h"
#include "AgentFrameworkCoreModule.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "Containers/Ticker.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "Animation/AgentFrameworkAnimationActions.h"
#include "BehaviorTree/AgentFrameworkBehaviorTreeActions.h"
#include "Blueprint/AgentFrameworkBlueprintActions.h"
#include "Build/AgentFrameworkBuildActions.h"
#include "Context/AgentFrameworkContextActions.h"
#include "Context/AgentFrameworkDiscoveryActions.h"
#include "Cpp/AgentFrameworkCppActions.h"
#include "DataTable/AgentFrameworkDataTableActions.h"
#include "Diagnostics/AgentFrameworkDiagnosticsActions.h"
#include "GAS/AgentFrameworkGASActions.h"
#include "Input/AgentFrameworkInputActions.h"
#include "Level/AgentFrameworkLevelActions.h"
#include "Material/AgentFrameworkMaterialActions.h"
#include "Media/AgentFrameworkMediaActions.h"
#include "Mesh/AgentFrameworkMeshActions.h"
#include "PCG/AgentFrameworkPCGActions.h"
#include "PIE/AgentFrameworkPIEActions.h"
#include "Performance/AgentFrameworkPerformanceActions.h"
#include "Python/AgentFrameworkPythonActions.h"
#include "Niagara/AgentFrameworkNiagaraActions.h"
#include "Sequencer/AgentFrameworkSequencerActions.h"
#include "Settings/AgentFrameworkSettingsActions.h"
#include "SourceControl/AgentFrameworkSourceControlActions.h"
#include "Validation/AgentFrameworkValidationActions.h"
#include "Viewport/AgentFrameworkViewportActions.h"
#include "Widget/AgentFrameworkWidgetActions.h"
#include "DataAsset/AgentFrameworkDataAssetActions.h"
#include "MetaSound/AgentFrameworkMetaSoundActions.h"
#include "AIAssistant/AgentFrameworkAIAssistantActions.h"
#include "AIAssistant/AIAssistantBridge.h"

TSharedPtr<FAgentFrameworkActionRouter> FAgentFrameworkHttpServer::ActionRouter = nullptr;
uint32 FAgentFrameworkHttpServer::Port = 18777;

void FAgentFrameworkHttpServer::Start()
{
	ActionRouter = MakeShared<FAgentFrameworkActionRouter>();
	RegisterAllExecutors(ActionRouter.ToSharedRef());

	TSharedPtr<IHttpRouter> Router = FHttpServerModule::Get().GetHttpRouter(Port);
	if (Router.IsValid())
	{
		Router->BindRoute(FHttpPath(TEXT("/api/tools")), EHttpServerRequestVerbs::VERB_GET, FHttpRequestHandler::CreateStatic(&FAgentFrameworkHttpServer::HandleListToolsRequest));
		Router->BindRoute(FHttpPath(TEXT("/api/execute_tool")), EHttpServerRequestVerbs::VERB_POST, FHttpRequestHandler::CreateStatic(&FAgentFrameworkHttpServer::HandleExecuteToolRequest));
		FHttpServerModule::Get().StartAllListeners();
	}
}

void FAgentFrameworkHttpServer::Stop()
{
	if (ActionRouter.IsValid())
	{
		if (FModuleManager::Get().IsModuleLoaded("HTTPServer"))
		{
			FHttpServerModule::Get().StopAllListeners();
		}
		ActionRouter.Reset();
	}
}

void FAgentFrameworkHttpServer::RegisterAllExecutors(TSharedRef<FAgentFrameworkActionRouter> InRouter)
{
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkAnimationActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkBehaviorTreeActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkBlueprintActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkBuildActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkContextActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkDiscoveryActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkCppActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkDataTableActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkDiagnosticsActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkGASActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkInputActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkLevelActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkMaterialActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkMediaActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkMeshActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkNiagaraActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkPCGActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkPIEActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkPerformanceActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkPythonActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkSequencerActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkSettingsActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkSourceControlActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkValidationActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkViewportActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkWidgetActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkDataAssetActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkAIAssistantActions>());
	InRouter->RegisterExecutor(MakeShared<FAgentFrameworkMetaSoundActions>());
}

bool FAgentFrameworkHttpServer::HandleListToolsRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("AgentFramework");
	if (!Plugin.IsValid())
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::ServerError));
		return true;
	}

	FString SchemaDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("ToolSchemas"));

	// Determine active skills
	FString ActiveSkillsPath = FPaths::Combine(FPaths::ProjectDir(), TEXT(".agents"), TEXT("active_skills.json"));
	if (!FPaths::FileExists(ActiveSkillsPath))
	{
		ActiveSkillsPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("active_skills.json"));
	}

	TArray<FString> ActiveSkills;
	if (FPaths::FileExists(ActiveSkillsPath))
	{
		FString SkillsContent;
		if (FFileHelper::LoadFileToString(SkillsContent, *ActiveSkillsPath))
		{
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SkillsContent);
			TArray<TSharedPtr<FJsonValue>> ArrayVal;
			if (FJsonSerializer::Deserialize(Reader, ArrayVal))
			{
				for (const auto& Val : ArrayVal)
				{
					if (Val.IsValid() && Val->Type == EJson::String)
					{
						ActiveSkills.Add(Val->AsString());
					}
				}
			}
		}
	}

	// Set of core tool schemas that are always loaded
	TSet<FString> CoreSchemas = {
		TEXT("context_tools.json"),
		TEXT("validation_tools.json"),
		TEXT("task_tools.json"),
		TEXT("meta_tools.json"),
		TEXT("build_tools.json"),
		TEXT("blueprint_tools.json"),
		TEXT("widget_tools.json"),
		TEXT("cpp_tools.json"),
		TEXT("python_tools.json"),
		TEXT("diagnostics_tools.json"),
		TEXT("input_tools.json"),
		TEXT("enhanced_input_tools.json"),
		TEXT("pie_tools.json"),
		TEXT("niagara_tools.json")
	};

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(SchemaDir, TEXT("*.json")), true, false);

	TArray<TSharedPtr<FJsonValue>> ToolList;
	for (const FString& File : Files)
	{
		// Filtering logic
		bool bShouldLoad = false;
		if (CoreSchemas.Contains(File))
		{
			bShouldLoad = true;
		}
		else
		{
			// Extract category name (e.g. "animation" from "animation_tools.json")
			FString Category = File;
			if (Category.EndsWith(TEXT("_tools.json"), ESearchCase::IgnoreCase))
			{
				Category = Category.LeftChop(11);
			}
			else if (Category.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
			{
				Category = Category.LeftChop(5);
			}

			bShouldLoad = ActiveSkills.Contains(Category);
		}

		if (!bShouldLoad)
		{
			continue;
		}

		FString FilePath = FPaths::Combine(SchemaDir, File);
		FString JsonContent;
		if (FFileHelper::LoadFileToString(JsonContent, *FilePath))
		{
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* InnerToolsArray = nullptr;
				if (JsonObj->TryGetArrayField(TEXT("tools"), InnerToolsArray) && InnerToolsArray)
				{
					for (const TSharedPtr<FJsonValue>& ToolVal : *InnerToolsArray)
					{
						TSharedPtr<FJsonObject> ToolObj = ToolVal->AsObject();
						if (ToolObj.IsValid())
						{
							const TSharedPtr<FJsonObject>* InputSchemaObj = nullptr;
							if (ToolObj->TryGetObjectField(TEXT("input_schema"), InputSchemaObj) && InputSchemaObj)
							{
								TSharedPtr<FJsonObject> InputSchema = *InputSchemaObj;
								ToolObj->RemoveField(TEXT("input_schema"));
								ToolObj->SetObjectField(TEXT("inputSchema"), InputSchema);
							}
						}
					}
				}
				ToolList.Add(MakeShared<FJsonValueObject>(JsonObj));
			}
		}
	}

	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetArrayField(TEXT("tools"), ToolList);

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseString, TEXT("application/json"));
	OnComplete(MoveTemp(Response));
	return true;
}

namespace
{
	struct FStartPIEWaiter : public TSharedFromThis<FStartPIEWaiter>
	{
		FHttpResultCallback OnComplete;
		FAgentFrameworkActionResult Result;
		FDelegateHandle PostPIEStartedHandle;
		FTSTicker::FDelegateHandle TimeoutTickerHandle;

		FStartPIEWaiter(const FHttpResultCallback& InOnComplete, const FAgentFrameworkActionResult& InResult)
			: OnComplete(InOnComplete)
			, Result(InResult)
		{
		}

		~FStartPIEWaiter()
		{
			Cleanup();
		}

		void Cleanup()
		{
			if (PostPIEStartedHandle.IsValid())
			{
				FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
				PostPIEStartedHandle.Reset();
			}
			if (TimeoutTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(TimeoutTickerHandle);
				TimeoutTickerHandle.Reset();
			}
		}

		void SendResponse()
		{
			Cleanup();

			TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
			ResultObj->SetBoolField(TEXT("bSuccess"), Result.bSuccess);
			ResultObj->SetBoolField(TEXT("bRequiresHumanVerification"), Result.bRequiresHumanVerification);
			ResultObj->SetStringField(TEXT("ResultMessage"), Result.ResultMessage);

			TArray<TSharedPtr<FJsonValue>> ErrorsArr;
			for (const FString& Error : Result.Errors) ErrorsArr.Add(MakeShared<FJsonValueString>(Error));
			ResultObj->SetArrayField(TEXT("Errors"), ErrorsArr);

			TArray<TSharedPtr<FJsonValue>> WarningsArr;
			for (const FString& Warning : Result.Warnings) WarningsArr.Add(MakeShared<FJsonValueString>(Warning));
			ResultObj->SetArrayField(TEXT("Warnings"), WarningsArr);

			FString ResponseString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
			FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

			TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseString, TEXT("application/json"));
			OnComplete(MoveTemp(Response));
		}
	};

	struct FQueryAIAssistantWaiter : public TSharedFromThis<FQueryAIAssistantWaiter>
	{
		FHttpResultCallback OnComplete;
		FAgentFrameworkActionResult Result;
		FTSTicker::FDelegateHandle TimeoutTickerHandle;

		FQueryAIAssistantWaiter(const FHttpResultCallback& InOnComplete, const FAgentFrameworkActionResult& InResult)
			: OnComplete(InOnComplete)
			, Result(InResult)
		{
		}

		~FQueryAIAssistantWaiter()
		{
			Cleanup();
		}

		void Cleanup()
		{
			if (TimeoutTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(TimeoutTickerHandle);
				TimeoutTickerHandle.Reset();
			}
		}

		void SendResponse()
		{
			Cleanup();

			TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
			ResultObj->SetBoolField(TEXT("bSuccess"), Result.bSuccess);
			ResultObj->SetBoolField(TEXT("bRequiresHumanVerification"), Result.bRequiresHumanVerification);
			ResultObj->SetStringField(TEXT("ResultMessage"), Result.ResultMessage);

			TArray<TSharedPtr<FJsonValue>> ErrorsArr;
			for (const FString& Error : Result.Errors) ErrorsArr.Add(MakeShared<FJsonValueString>(Error));
			ResultObj->SetArrayField(TEXT("Errors"), ErrorsArr);

			TArray<TSharedPtr<FJsonValue>> WarningsArr;
			for (const FString& Warning : Result.Warnings) WarningsArr.Add(MakeShared<FJsonValueString>(Warning));
			ResultObj->SetArrayField(TEXT("Warnings"), WarningsArr);

			FString ResponseString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
			FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

			TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseString, TEXT("application/json"));
			OnComplete(MoveTemp(Response));
		}
	};
}

bool FAgentFrameworkHttpServer::HandleExecuteToolRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FString BodyString;
	if (Request.Body.Num() > 0)
	{
		FUTF8ToTCHAR TCharData(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
		BodyString = FString(TCharData.Length(), TCharData.Get());
	}

	TSharedPtr<FJsonObject> RequestObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyString);
	if (!FJsonSerializer::Deserialize(Reader, RequestObj) || !RequestObj.IsValid())
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest));
		return true;
	}

	FAgentFrameworkToolCall ToolCall;
	ToolCall.ToolCallId = FGuid::NewGuid().ToString();
	if (!RequestObj->TryGetStringField(TEXT("name"), ToolCall.ToolName) || ToolCall.ToolName.IsEmpty())
	{
		if (!RequestObj->TryGetStringField(TEXT("tool"), ToolCall.ToolName) || ToolCall.ToolName.IsEmpty())
		{
			OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest));
			return true;
		}
	}

	const TSharedPtr<FJsonObject>* ArgumentsObj = nullptr;
	if (RequestObj->TryGetObjectField(TEXT("arguments"), ArgumentsObj) && ArgumentsObj)
	{
		ToolCall.InputParams = *ArgumentsObj;
	}
	else if (RequestObj->TryGetObjectField(TEXT("parameters"), ArgumentsObj) && ArgumentsObj)
	{
		ToolCall.InputParams = *ArgumentsObj;
	}
	else
	{
		ToolCall.InputParams = MakeShared<FJsonObject>();
	}

	if (!ActionRouter.IsValid())
	{
		FAgentFrameworkActionResult Result;
		Result.bSuccess = false;
		Result.ResultMessage = TEXT("Action Router not available");

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("bSuccess"), Result.bSuccess);
		ResultObj->SetStringField(TEXT("ResultMessage"), Result.ResultMessage);

		FString ResponseString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
		FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseString, TEXT("application/json"));
		OnComplete(MoveTemp(Response));
		return true;
	}

	ActionRouter->RouteToolCallAsync(ToolCall, [OnComplete, ToolCall](FAgentFrameworkActionResult Result) {
		// Intercept start_pie_session to wait for it to actually start
		if (ToolCall.ToolName == TEXT("start_pie_session") && Result.bSuccess)
		{
			if (GEditor && !GEditor->IsPlaySessionInProgress())
			{
				TSharedRef<FStartPIEWaiter> Waiter = MakeShared<FStartPIEWaiter>(OnComplete, Result);

				Waiter->PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddLambda([Waiter](bool bIsSimulating)
				{
					Waiter->Result.bSuccess = true;
					Waiter->Result.ResultMessage = TEXT("PIE session started successfully.");
					Waiter->SendResponse();
				});

				double StartTime = FPlatformTime::Seconds();
				Waiter->TimeoutTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Waiter, StartTime](float DeltaTime) -> bool
				{
					if (FPlatformTime::Seconds() - StartTime > 10.0)
					{
						Waiter->Result.bSuccess = false;
						Waiter->Result.Errors.Add(TEXT("Timed out waiting for PIE session to start (10s)."));
						Waiter->SendResponse();
						return false;
					}
					return true;
				}));

				return; // Defer response
			}
		}

		// Intercept query_epic_assistant to wait for AIAssistant response
		if (ToolCall.ToolName == TEXT("query_epic_assistant") && Result.bSuccess)
		{
			UAIAssistantBridge* Bridge = FAgentFrameworkAIAssistantActions::GetBridgeInstance();
			if (Bridge)
			{
				TSharedRef<FQueryAIAssistantWaiter> Waiter = MakeShared<FQueryAIAssistantWaiter>(OnComplete, Result);
				
				bool bQueryStarted = Bridge->SendQuery(Bridge->GetActiveQueryPrompt(), FAIAssistantResponseDelegate::CreateLambda([Waiter](const FString& ResponseText, bool bQuerySuccess)
				{
					Waiter->Result.bSuccess = bQuerySuccess;
					if (bQuerySuccess)
					{
						Waiter->Result.ResultMessage = ResponseText;
					}
					else
					{
						Waiter->Result.Errors.Add(ResponseText);
						FString LowerResp = ResponseText.ToLower();
						if (LowerResp.Contains(TEXT("captcha")) || LowerResp.Contains(TEXT("verification")) || LowerResp.Contains(TEXT("security check")) || LowerResp.Contains(TEXT("human")))
						{
							Waiter->Result.bRequiresHumanVerification = true;
							Async(EAsyncExecution::TaskGraphMainThread, []()
							{
								FNotificationInfo Info(FText::FromString(
									TEXT("AI Agent Action Blocked: Epic AI Assistant requires human verification (CAPTCHA). Please complete verification in Unreal Editor.")));
								Info.bUseSuccessFailIcons = true;
								Info.ExpireDuration = 10.0f;
								Info.bFireAndForget = true;
								FSlateNotificationManager::Get().AddNotification(Info);
							});
						}
					}
					Waiter->SendResponse();
				}));

				if (bQueryStarted)
				{
					double StartTime = FPlatformTime::Seconds();
					Waiter->TimeoutTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Waiter, StartTime](float DeltaTime) -> bool
					{
						if (FPlatformTime::Seconds() - StartTime > 30.0)
						{
							Waiter->Result.bSuccess = false;
							Waiter->Result.Errors.Add(TEXT("Timed out waiting for Epic AI Assistant response (30s)."));
							Waiter->SendResponse();
							return false;
						}
						return true;
					}));

					return; // Defer response
				}
				else
				{
					Result.bSuccess = false;
					Result.Errors.Add(TEXT("Failed to send query via AIAssistantBridge. Make sure the AI Assistant tab is open."));
				}
			}
		}

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("bSuccess"), Result.bSuccess);
		ResultObj->SetBoolField(TEXT("bRequiresHumanVerification"), Result.bRequiresHumanVerification);
		ResultObj->SetStringField(TEXT("ResultMessage"), Result.ResultMessage);
		
		TArray<TSharedPtr<FJsonValue>> ErrorsArr;
		for (const FString& Error : Result.Errors) ErrorsArr.Add(MakeShared<FJsonValueString>(Error));
		ResultObj->SetArrayField(TEXT("Errors"), ErrorsArr);

		TArray<TSharedPtr<FJsonValue>> WarningsArr;
		for (const FString& Warning : Result.Warnings) WarningsArr.Add(MakeShared<FJsonValueString>(Warning));
		ResultObj->SetArrayField(TEXT("Warnings"), WarningsArr);

		FString ResponseString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
		FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);

		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseString, TEXT("application/json"));
		OnComplete(MoveTemp(Response));
	});

	return true;
}

