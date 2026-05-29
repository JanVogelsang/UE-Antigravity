// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityHttpServer.h"
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
#include "AntigravitySkillsManager.h"
#include "AntigravitySettings.h"
#include "AntigravityCoreModule.h"
#include "Modules/ModuleManager.h"

#include "Animation/AntigravityAnimationActions.h"
#include "BehaviorTree/AntigravityBehaviorTreeActions.h"
#include "Blueprint/AntigravityBlueprintActions.h"
#include "Context/AntigravityContextActions.h"
#include "Cpp/AntigravityCppActions.h"
#include "DataTable/AntigravityDataTableActions.h"
#include "Diagnostics/AntigravityDiagnosticsActions.h"
#include "GAS/AntigravityGASActions.h"
#include "Input/AntigravityInputActions.h"
#include "Level/AntigravityLevelActions.h"
#include "Material/AntigravityMaterialActions.h"
#include "Media/AntigravityMediaActions.h"
#include "Mesh/AntigravityMeshActions.h"
#include "PCG/AntigravityPCGActions.h"
#include "PIE/AntigravityPIEActions.h"
#include "Performance/AntigravityPerformanceActions.h"
#include "Python/AntigravityPythonActions.h"
#include "Niagara/AntigravityNiagaraActions.h"
#include "Sequencer/AntigravitySequencerActions.h"
#include "Settings/AntigravitySettingsActions.h"
#include "SourceControl/AntigravitySourceControlActions.h"
#include "Validation/AntigravityValidationActions.h"
#include "Viewport/AntigravityViewportActions.h"
#include "Widget/AntigravityWidgetActions.h"
#include "DataAsset/AntigravityDataAssetActions.h"

TSharedPtr<FAntigravityActionRouter> FAntigravityHttpServer::ActionRouter = nullptr;
uint32 FAntigravityHttpServer::Port = 18777;

void FAntigravityHttpServer::Start()
{
	ActionRouter = MakeShared<FAntigravityActionRouter>();
	RegisterAllExecutors(ActionRouter.ToSharedRef());

	TSharedPtr<IHttpRouter> Router = FHttpServerModule::Get().GetHttpRouter(Port);
	if (Router.IsValid())
	{
		Router->BindRoute(FHttpPath(TEXT("/api/tools")), EHttpServerRequestVerbs::VERB_GET, FHttpRequestHandler::CreateStatic(&FAntigravityHttpServer::HandleListToolsRequest));
		Router->BindRoute(FHttpPath(TEXT("/api/execute_tool")), EHttpServerRequestVerbs::VERB_POST, FHttpRequestHandler::CreateStatic(&FAntigravityHttpServer::HandleExecuteToolRequest));
		Router->BindRoute(FHttpPath(TEXT("/api/skills")), EHttpServerRequestVerbs::VERB_GET, FHttpRequestHandler::CreateStatic(&FAntigravityHttpServer::HandleGetSkillsRequest));
		FHttpServerModule::Get().StartAllListeners();
	}
}

void FAntigravityHttpServer::Stop()
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

void FAntigravityHttpServer::RegisterAllExecutors(TSharedRef<FAntigravityActionRouter> InRouter)
{
	InRouter->RegisterExecutor(MakeShared<FAntigravityAnimationActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityBehaviorTreeActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityBlueprintActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityContextActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityCppActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityDataTableActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityDiagnosticsActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityGASActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityInputActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityLevelActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityMaterialActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityMediaActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityMeshActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityNiagaraActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityPCGActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityPIEActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityPerformanceActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityPythonActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravitySequencerActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravitySettingsActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravitySourceControlActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityValidationActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityViewportActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityWidgetActions>());
	InRouter->RegisterExecutor(MakeShared<FAntigravityDataAssetActions>());
}

bool FAntigravityHttpServer::HandleListToolsRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("Antigravity");
	if (!Plugin.IsValid())
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::ServerError));
		return true;
	}

	FString SchemaDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("ToolSchemas"));

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(SchemaDir, TEXT("*.json")), true, false);

	TArray<TSharedPtr<FJsonValue>> ToolList;
	for (const FString& File : Files)
	{
		FString FilePath = FPaths::Combine(SchemaDir, File);
		FString JsonContent;
		if (FFileHelper::LoadFileToString(JsonContent, *FilePath))
		{
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				if (JsonObj->HasField(TEXT("input_schema")))
				{
					TSharedPtr<FJsonObject> InputSchema = JsonObj->GetObjectField(TEXT("input_schema"));
					JsonObj->RemoveField(TEXT("input_schema"));
					JsonObj->SetObjectField(TEXT("inputSchema"), InputSchema);
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

bool FAntigravityHttpServer::HandleExecuteToolRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
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

	FAntigravityToolCall ToolCall;
	ToolCall.ToolCallId = FGuid::NewGuid().ToString();
	ToolCall.ToolName = RequestObj->GetStringField(TEXT("name"));
	ToolCall.InputParams = RequestObj->GetObjectField(TEXT("arguments"));

	AsyncTask(ENamedThreads::GameThread, [ToolCall, OnComplete]() {


		FAntigravityActionResult Result;
		if (ActionRouter.IsValid())
		{
			Result = ActionRouter->RouteToolCall(ToolCall);
		}
		else
		{
			Result.bSuccess = false;
			Result.ResultMessage = TEXT("Action Router not available");
		}



		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("bSuccess"), Result.bSuccess);
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

bool FAntigravityHttpServer::HandleGetSkillsRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FAntigravitySkillsManager SkillsManager;
	SkillsManager.Initialize();

	FString SkillsContext = SkillsManager.GetAllSkillsContextString();

	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetStringField(TEXT("skills_context"), SkillsContext);

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseString, TEXT("application/json"));
	OnComplete(MoveTemp(Response));
	return true;
}
