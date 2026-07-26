// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Context/AgentFrameworkDiscoveryActions.h"
#include "AgentFrameworkActionUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

FAgentFrameworkDiscoveryActions::FAgentFrameworkDiscoveryActions() {}
FAgentFrameworkDiscoveryActions::~FAgentFrameworkDiscoveryActions() {}

FName FAgentFrameworkDiscoveryActions::GetActionName() const { return FName(TEXT("Discovery")); }

TArray<FString> FAgentFrameworkDiscoveryActions::GetSupportedToolNames() const
{
	return {
		TEXT("get_tool_info"),
		TEXT("list_tools_in_category")
	};
}

bool FAgentFrameworkDiscoveryActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkDiscoveryActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	TSharedPtr<FJsonObject> ParamsPtr = Params;
	TArray<FString> ParseErrors;
	UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("_tool_name"), ToolName, ParseErrors, false);
	if (ToolName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("tool_name"), ToolName, ParseErrors, false);
	}

	if (!ToolName.IsEmpty())
	{
		if (ToolName == TEXT("get_tool_info"))
		{
			Result = ExecuteGetToolInfo(Params, Result);
		}
		else if (ToolName == TEXT("list_tools_in_category"))
		{
			Result = ExecuteListToolsInCategory(Params, Result);
		}
	}
	else
	{
		Result.Errors.Add(TEXT("Could not determine discovery action."));
	}

	if (Result.bSuccess)
	{
#if WITH_EDITOR
		if (GEditor)
		{
			USoundBase* SuccessSound = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
			if (IsValid(SuccessSound))
			{
				GEditor->PlayEditorSound(SuccessSound);
			}
		}
#endif
	}

	return Result;
}

static bool MatchCategory(const FString& Category, const FString& Domain)
{
	FString CatLower = Category.ToLower().Replace(TEXT("_"), TEXT("")).Replace(TEXT(" "), TEXT(""));
	FString DomLower = Domain.ToLower().Replace(TEXT("_"), TEXT("")).Replace(TEXT(" "), TEXT(""));

	// Special cases
	if (CatLower == TEXT("behavior"))
	{
		return DomLower.Contains(TEXT("behaviortree")) || DomLower.Contains(TEXT("behavior"));
	}
	if (CatLower == TEXT("input"))
	{
		return DomLower.Contains(TEXT("input"));
	}
	if (CatLower == TEXT("data"))
	{
		return DomLower.Contains(TEXT("dataasset")) || DomLower.Contains(TEXT("datatable"));
	}

	return DomLower.Contains(CatLower) || CatLower.Contains(DomLower);
}

FAgentFrameworkActionResult FAgentFrameworkDiscoveryActions::ExecuteGetToolInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString TargetToolName;
	TSharedPtr<FJsonObject> ParamsPtr = Params;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("tool_name"), TargetToolName, Result.Errors, true))
	{
		return Result;
	}

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("AgentFramework");
	if (!Plugin.IsValid())
	{
		Result.Errors.Add(TEXT("Plugin 'AgentFramework' not found."));
		return Result;
	}

	FString SchemaDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("ToolSchemas"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(SchemaDir, TEXT("*.json")), true, false);

	TSharedPtr<FJsonObject> FoundToolObj = nullptr;
	TArray<FString> RelatedTools;

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
				const TArray<TSharedPtr<FJsonValue>>* ToolsArray = nullptr;
				TArray<FString> TempErrors;
				if (UAgentFrameworkActionUtils::TryGetArrayParam(JsonObj, TEXT("tools"), ToolsArray, TempErrors, false) && ToolsArray)
				{
					TSharedPtr<FJsonObject> TargetTool = nullptr;
					for (const auto& ToolVal : *ToolsArray)
					{
						TSharedPtr<FJsonObject> ToolObj = ToolVal->AsObject();
						if (ToolObj.IsValid())
						{
							FString Name;
							if (UAgentFrameworkActionUtils::TryGetStringParam(ToolObj, TEXT("name"), Name, TempErrors, false) && Name == TargetToolName)
							{
								TargetTool = ToolObj;
							}
						}
					}

					if (TargetTool.IsValid())
					{
						FoundToolObj = TargetTool;
						for (const auto& ToolVal : *ToolsArray)
						{
							TSharedPtr<FJsonObject> ToolObj = ToolVal->AsObject();
							if (ToolObj.IsValid())
							{
								FString Name;
								if (UAgentFrameworkActionUtils::TryGetStringParam(ToolObj, TEXT("name"), Name, TempErrors, false) && Name != TargetToolName)
								{
									RelatedTools.Add(Name);
								}
							}
						}
						break;
					}
				}
			}
		}
	}

	if (!FoundToolObj.IsValid())
	{
		Result.Errors.Add(FString::Printf(TEXT("Tool '%s' not found."), *TargetToolName));
		return Result;
	}

	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetObjectField(TEXT("tool"), FoundToolObj);

	TArray<TSharedPtr<FJsonValue>> RelatedArr;
	for (const FString& Rel : RelatedTools)
	{
		RelatedArr.Add(MakeShared<FJsonValueString>(Rel));
	}
	ResponseObj->SetArrayField(TEXT("related_tools"), RelatedArr);

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseString;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkDiscoveryActions::ExecuteListToolsInCategory(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString Category;
	TSharedPtr<FJsonObject> ParamsPtr = Params;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("category"), Category, Result.Errors, true))
	{
		return Result;
	}

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("AgentFramework");
	if (!Plugin.IsValid())
	{
		Result.Errors.Add(TEXT("Plugin 'AgentFramework' not found."));
		return Result;
	}

	FString SchemaDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("ToolSchemas"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(SchemaDir, TEXT("*.json")), true, false);

	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ToolSummaryList;

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
				FString Domain;
				TArray<FString> TempErrors;
				UAgentFrameworkActionUtils::TryGetStringParam(JsonObj, TEXT("domain"), Domain, TempErrors, false);
				if (MatchCategory(Category, Domain))
				{
					const TArray<TSharedPtr<FJsonValue>>* ToolsArray = nullptr;
					if (UAgentFrameworkActionUtils::TryGetArrayParam(JsonObj, TEXT("tools"), ToolsArray, TempErrors, false) && ToolsArray)
					{
						for (const auto& ToolVal : *ToolsArray)
						{
							TSharedPtr<FJsonObject> ToolObj = ToolVal->AsObject();
							if (ToolObj.IsValid())
							{
								FString Name, Description;
								UAgentFrameworkActionUtils::TryGetStringParam(ToolObj, TEXT("name"), Name, TempErrors, false);
								UAgentFrameworkActionUtils::TryGetStringParam(ToolObj, TEXT("description"), Description, TempErrors, false);

								TSharedPtr<FJsonObject> SummaryObj = MakeShared<FJsonObject>();
								SummaryObj->SetStringField(TEXT("name"), Name);
								SummaryObj->SetStringField(TEXT("description"), Description);
								ToolSummaryList.Add(MakeShared<FJsonValueObject>(SummaryObj));
							}
						}
					}
				}
			}
		}
	}

	ResponseObj->SetArrayField(TEXT("tools"), ToolSummaryList);

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseString;
	return Result;
}
