// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Context/AgentFrameworkContextActions.h"
#include "AgentFrameworkCoreModule.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"



FAgentFrameworkContextActions::FAgentFrameworkContextActions() {}
FAgentFrameworkContextActions::~FAgentFrameworkContextActions() {}

FName FAgentFrameworkContextActions::GetActionName() const { return FName(TEXT("Context")); }

TArray<FString> FAgentFrameworkContextActions::GetSupportedToolNames() const
{
	return {
		TEXT("search_assets"),
		TEXT("list_directory"),
		TEXT("read_file_snippet"),
		TEXT("activate_skill")
	};
}

bool FAgentFrameworkContextActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkContextActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty())
	{
		Params->TryGetStringField(TEXT("tool_name"), Action);
	}

	if (Action == TEXT("search_assets"))
	{
		return ExecuteSearchAssets(Params, Result);
	}
	else if (Action == TEXT("list_directory"))
	{
		return ExecuteListDirectory(Params, Result);
	}
	else if (Action == TEXT("read_file_snippet"))
	{
		return ExecuteReadFileSnippet(Params, Result);
	}
	else if (Action == TEXT("activate_skill"))
	{
		return ExecuteActivateSkill(Params, Result);
	}

	Result.Errors.Add(TEXT("Could not determine context action."));
	return Result;
}



// ============================================================================
// search_assets: Search the asset registry by class, name, or path
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkContextActions::ExecuteSearchAssets(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString Query, ClassFilter, PathFilter;
	Params->TryGetStringField(TEXT("query"), Query);
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);
	Params->TryGetStringField(TEXT("path_filter"), PathFilter);

	int32 MaxResults = 50;
	Params->TryGetNumberField(TEXT("max_results"), MaxResults);
	MaxResults = FMath::Clamp(MaxResults, 1, 200);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAllAssets(AllAssets, true);

	FString Output = TEXT("=== Asset Search Results ===\n");
	int32 MatchCount = 0;

	for (const FAssetData& Asset : AllAssets)
	{
		if (MatchCount >= MaxResults) break;

		FString AssetPath = Asset.GetObjectPathString();
		FString AssetName = Asset.AssetName.ToString();
		FString AssetClass = Asset.AssetClassPath.GetAssetName().ToString();

		// Only project assets
		if (!AssetPath.StartsWith(TEXT("/Game/"))) continue;

		// Apply filters
		if (!ClassFilter.IsEmpty() && !AssetClass.Contains(ClassFilter, ESearchCase::IgnoreCase))
			continue;
		if (!PathFilter.IsEmpty() && !AssetPath.Contains(PathFilter, ESearchCase::IgnoreCase))
			continue;
		if (!Query.IsEmpty() && !AssetName.Contains(Query, ESearchCase::IgnoreCase) && !AssetPath.Contains(Query, ESearchCase::IgnoreCase))
			continue;

		Output += FString::Printf(TEXT("  %s [%s] â€” %s\n"), *AssetName, *AssetClass, *AssetPath);
		MatchCount++;
	}

	if (MatchCount == 0)
	{
		Output += TEXT("  (no matching assets found)\n");
	}
	else
	{
		Output += FString::Printf(TEXT("\nFound %d matches"), MatchCount);
		if (MatchCount >= MaxResults)
		{
			Output += FString::Printf(TEXT(" (capped at %d, use filters to narrow)"), MaxResults);
		}
		Output += TEXT("\n");
	}

	Result.bSuccess = true;
	Result.ResultMessage = Output;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkContextActions::ExecuteListDirectory(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString RelativePath;
	if (!Params->TryGetStringField(TEXT("directory"), RelativePath))
	{
		Params->TryGetStringField(TEXT("relative_path"), RelativePath);
	}

	FString AbsolutePath = FPaths::ProjectDir() / RelativePath;
	FPaths::NormalizeDirectoryName(AbsolutePath);

	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.DirectoryExists(*AbsolutePath))
	{
		Result.Errors.Add(FString::Printf(TEXT("Directory does not exist: %s"), *AbsolutePath));
		return Result;
	}

	TArray<FString> Files;
	TArray<FString> Directories;
	FileManager.FindFiles(Files, *(AbsolutePath / TEXT("*")), true, false);
	FileManager.FindFiles(Directories, *(AbsolutePath / TEXT("*")), false, true);

	FString Output = FString::Printf(TEXT("Contents of directory: %s\n\nDirectories:\n"), *RelativePath);
	for (const FString& Dir : Directories)
	{
		Output += FString::Printf(TEXT("  [DIR]  %s\n"), *Dir);
	}
	Output += TEXT("\nFiles:\n");
	for (const FString& File : Files)
	{
		Output += FString::Printf(TEXT("         %s\n"), *File);
	}

	Result.bSuccess = true;
	Result.ResultMessage = Output;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkContextActions::ExecuteReadFileSnippet(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString RelativePath;
	if (!Params->TryGetStringField(TEXT("file_path"), RelativePath))
	{
		if (!Params->TryGetStringField(TEXT("relative_path"), RelativePath))
		{
			Result.Errors.Add(TEXT("Missing file_path."));
			return Result;
		}
	}

	int32 StartLine = 1;
	int32 EndLine = 100;
	Params->TryGetNumberField(TEXT("start_line"), StartLine);
	Params->TryGetNumberField(TEXT("end_line"), EndLine);

	FString AbsolutePath = FPaths::ProjectDir() / RelativePath;
	FPaths::NormalizeFilename(AbsolutePath);

	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *AbsolutePath))
	{
		Result.Errors.Add(FString::Printf(TEXT("Could not read file: %s"), *AbsolutePath));
		return Result;
	}

	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines, false);

	if (StartLine < 1) StartLine = 1;
	if (EndLine > Lines.Num()) EndLine = Lines.Num();

	FString Output = FString::Printf(TEXT("File: %s (lines %d to %d of %d)\n\n"), *RelativePath, StartLine, EndLine, Lines.Num());
	for (int32 i = StartLine - 1; i < EndLine; ++i)
	{
		Output += FString::Printf(TEXT("%d: %s\n"), i + 1, *Lines[i]);
	}

	Result.bSuccess = true;
	Result.ResultMessage = Output;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkContextActions::ExecuteActivateSkill(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	// 1. Get skill names to add
	TArray<FString> SkillsToAdd;
	FString SingleSkill;
	if (Params->TryGetStringField(TEXT("skill_name"), SingleSkill) && !SingleSkill.IsEmpty())
	{
		SkillsToAdd.Add(SingleSkill);
	}
	const TArray<TSharedPtr<FJsonValue>>* SkillsArray = nullptr;
	if (Params->TryGetArrayField(TEXT("skills"), SkillsArray) && SkillsArray)
	{
		for (const TSharedPtr<FJsonValue>& Val : *SkillsArray)
		{
			if (Val.IsValid() && Val->Type == EJson::String)
			{
				FString SkillStr = Val->AsString();
				if (!SkillStr.IsEmpty())
				{
					SkillsToAdd.Add(SkillStr);
				}
			}
		}
	}

	if (SkillsToAdd.Num() == 0)
	{
		Result.Errors.Add(TEXT("No skill name or skills array specified."));
		return Result;
	}

	// 2. Read existing skills to merge
	FString ActiveSkillsDir = FPaths::Combine(FPaths::ProjectDir(), TEXT(".agents"));
	FString ActiveSkillsPath = FPaths::Combine(ActiveSkillsDir, TEXT("active_skills.json"));

	TArray<FString> CurrentSkills;
	if (FPaths::FileExists(ActiveSkillsPath))
	{
		FString SkillsContent;
		if (FFileHelper::LoadFileToString(SkillsContent, *ActiveSkillsPath))
		{
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SkillsContent);
			TArray<TSharedPtr<FJsonValue>> ArrayVal;
			if (FJsonSerializer::Deserialize(Reader, ArrayVal))
			{
				for (const auto& Val : ArrayVal)
				{
					if (Val.IsValid() && Val->Type == EJson::String)
					{
						CurrentSkills.AddUnique(Val->AsString());
					}
				}
			}
		}
	}

	// 3. Merge new skills
	for (const FString& NewSkill : SkillsToAdd)
	{
		CurrentSkills.AddUnique(NewSkill);
	}

	// 4. Ensure .agents directory exists
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.DirectoryExists(*ActiveSkillsDir))
	{
		FileManager.MakeDirectory(*ActiveSkillsDir, true);
	}

	// 5. Write back to active_skills.json as a JSON array of strings
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (const FString& Skill : CurrentSkills)
	{
		JsonArray.Add(MakeShared<FJsonValueString>(Skill));
	}

	FString OutJsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
	FJsonSerializer::Serialize(JsonArray, Writer);

	if (FFileHelper::SaveStringToFile(OutJsonString, *ActiveSkillsPath))
	{
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Successfully activated skills. Current active skills: %s"), *OutJsonString);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to write to file: %s"), *ActiveSkillsPath));
	}

	return Result;
}


