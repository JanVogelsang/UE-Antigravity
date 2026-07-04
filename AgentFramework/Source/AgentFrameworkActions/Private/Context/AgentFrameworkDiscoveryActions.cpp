// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Context/AgentFrameworkDiscoveryActions.h"
#include "AgentFrameworkCoreModule.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

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
	if (Params->TryGetStringField(TEXT("_tool_name"), ToolName) || Params->TryGetStringField(TEXT("tool_name"), ToolName))
	{
		if (ToolName == TEXT("get_tool_info"))
		{
			return ExecuteGetToolInfo(Params, Result);
		}
		else if (ToolName == TEXT("list_tools_in_category"))
		{
			return ExecuteListToolsInCategory(Params, Result);
		}
	}

	Result.Errors.Add(TEXT("Could not determine discovery action."));
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
	if (!Params->TryGetStringField(TEXT("tool_name"), TargetToolName) || TargetToolName.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing 'tool_name' parameter."));
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
				if (JsonObj->TryGetArrayField(TEXT("tools"), ToolsArray))
				{
					TSharedPtr<FJsonObject> TargetTool = nullptr;
					for (const auto& ToolVal : *ToolsArray)
					{
						TSharedPtr<FJsonObject> ToolObj = ToolVal->AsObject();
						if (ToolObj.IsValid())
						{
							FString Name;
							if (ToolObj->TryGetStringField(TEXT("name"), Name) && Name == TargetToolName)
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
								if (ToolObj->TryGetStringField(TEXT("name"), Name) && Name != TargetToolName)
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
	if (!Params->TryGetStringField(TEXT("category"), Category) || Category.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing 'category' parameter."));
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
				JsonObj->TryGetStringField(TEXT("domain"), Domain);
				if (MatchCategory(Category, Domain))
				{
					const TArray<TSharedPtr<FJsonValue>>* ToolsArray = nullptr;
					if (JsonObj->TryGetArrayField(TEXT("tools"), ToolsArray))
					{
						for (const auto& ToolVal : *ToolsArray)
						{
							TSharedPtr<FJsonObject> ToolObj = ToolVal->AsObject();
							if (ToolObj.IsValid())
							{
								FString Name, Description;
								ToolObj->TryGetStringField(TEXT("name"), Name);
								ToolObj->TryGetStringField(TEXT("description"), Description);

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
