// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravitySkillsManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"


FAntigravitySkillsManager::FAntigravitySkillsManager()
{
}

FAntigravitySkillsManager::~FAntigravitySkillsManager()
{
}

void FAntigravitySkillsManager::Initialize()
{
	// Load global skills from Resources/Skills/*.md
	const FString SkillsDir = GetSkillsDirectory();
	if (FPaths::DirectoryExists(SkillsDir))
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(SkillsDir, TEXT("*.md")), true, false);
		for (const FString& FileName : Files)
		{
			LoadSkillFromFile(FPaths::Combine(SkillsDir, FileName));
		}
	}

	// Load mode-specific skills from Resources/Skills-[mode]/*.md
	// These skills are only available when in the matching mode
	static const TArray<FString> ModeNames = {
		TEXT("general"), TEXT("blueprint"), TEXT("cpp_code"),
		TEXT("architect"), TEXT("debug"), TEXT("asset"), TEXT("orchestrator")
	};

	FString PluginBaseDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Antigravity"));
	for (const FString& ModeName : ModeNames)
	{
		const FString ModeSkillsDir = FPaths::Combine(
			PluginBaseDir, TEXT("Resources"),
			FString::Printf(TEXT("Skills-%s"), *ModeName)
		);
		if (FPaths::DirectoryExists(ModeSkillsDir))
		{
			TArray<FString> ModeFiles;
			IFileManager::Get().FindFiles(ModeFiles, *FPaths::Combine(ModeSkillsDir, TEXT("*.md")), true, false);
			for (const FString& FileName : ModeFiles)
			{
				FAntigravitySkill Skill = ParseSkillDocument(
					FString(),  // will load from file
					FPaths::Combine(ModeSkillsDir, FileName)
				);
				FString Content;
				if (FFileHelper::LoadFileToString(Content, *FPaths::Combine(ModeSkillsDir, FileName)))
				{
					Skill = ParseSkillDocument(Content, FPaths::Combine(ModeSkillsDir, FileName));
					Skill.Tags.AddUnique(FString::Printf(TEXT("mode:%s"), *ModeName));
					if (!Skill.Name.IsEmpty())
					{
						Skills.Add(Skill);
					}
				}
			}
		}
	}

	// Load project-level skills from [ProjectDir]/.antigravity/skills/*.md
	// These override global skills for this specific project
	const FString ProjectSkillsDir = FPaths::Combine(
		FPaths::ProjectDir(), TEXT(".antigravity"), TEXT("skills")
	);
	if (FPaths::DirectoryExists(ProjectSkillsDir))
	{
		TArray<FString> ProjectFiles;
		IFileManager::Get().FindFiles(ProjectFiles, *FPaths::Combine(ProjectSkillsDir, TEXT("*.md")), true, false);
		for (const FString& FileName : ProjectFiles)
		{
			FString FilePath = FPaths::Combine(ProjectSkillsDir, FileName);
			FString Content;
			if (FFileHelper::LoadFileToString(Content, *FilePath))
			{
				FAntigravitySkill Skill = ParseSkillDocument(Content, FilePath);
				Skill.Tags.AddUnique(TEXT("source:project"));
				if (!Skill.Name.IsEmpty())
				{
					// Project skills override global skills with same name
					Skills.RemoveAll([&](const FAntigravitySkill& S) { return S.Name == Skill.Name; });
					Skills.Add(Skill);
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("AntigravitySkillsManager: Loaded project skills from %s"), *ProjectSkillsDir);
	}

	UE_LOG(LogTemp, Log, TEXT("AntigravitySkillsManager: Loaded %d total skills."), Skills.Num());
}

FString FAntigravitySkillsManager::GetSkillsDirectory()
{
	FString PluginBaseDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Antigravity"));
	return FPaths::Combine(PluginBaseDir, TEXT("Resources"), TEXT("Skills"));
}

void FAntigravitySkillsManager::LoadSkillFromFile(const FString& FilePath)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *FilePath)) return;

	FAntigravitySkill Skill = ParseSkillDocument(Content, FilePath);
	if (!Skill.Name.IsEmpty())
	{
		Skills.Add(Skill);
	}
}

FAntigravitySkill FAntigravitySkillsManager::ParseSkillDocument(
	const FString& Content, const FString& FilePath) const
{
	FAntigravitySkill Skill;
	Skill.Content = Content;
	Skill.FilePath = FilePath;

	// Name from filename (without extension)
	Skill.Name = FPaths::GetBaseFilename(FilePath).ToLower();

	// Try to extract display name and description from first lines
	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines, false);

	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("# Skill:")))
		{
			Skill.DisplayName = Line.Mid(9).TrimStartAndEnd();
		}
		else if (Line.StartsWith(TEXT("## Description")) && Skill.Description.IsEmpty())
		{
			// Description is next non-empty line
		}
		else if (!Skill.DisplayName.IsEmpty() && Skill.Description.IsEmpty()
			&& !Line.StartsWith(TEXT("#")) && !Line.IsEmpty())
		{
			Skill.Description = Line.TrimStartAndEnd();
		}

		if (!Skill.DisplayName.IsEmpty() && !Skill.Description.IsEmpty())
		{
			break;
		}
	}

	if (Skill.DisplayName.IsEmpty())
	{
		Skill.DisplayName = Skill.Name;
	}

	return Skill;
}

bool FAntigravitySkillsManager::GetSkillContent(
	const FString& SkillName,
	const FString& Args,
	FString& OutContent
) const
{
	for (const FAntigravitySkill& Skill : Skills)
	{
		if (Skill.Name.Equals(SkillName, ESearchCase::IgnoreCase))
		{
			OutContent = Skill.Content;
			OutContent.ReplaceInline(TEXT("{{arg}}"), *Args);

			// Handle multiple arguments
			TArray<FString> ArgList;
			Args.ParseIntoArray(ArgList, TEXT(" "));
			for (int32 i = 0; i < ArgList.Num(); i++)
			{
				const FString Placeholder = FString::Printf(TEXT("{{arg%d}}"), i + 1);
				OutContent.ReplaceInline(*Placeholder, *ArgList[i]);
			}

			return true;
		}
	}
	return false;
}

FString FAntigravitySkillsManager::GetAllSkillsContextString() const
{
	FString Output = TEXT("=== Unreal Engine Built-in Skills ===\n");
	Output += TEXT("The following skills are available for you to reference. Use them to understand project-specific workflows or standard UE best practices.\n\n");

	for (const FAntigravitySkill& Skill : Skills)
	{
		Output += FString::Printf(TEXT("# Skill: %s\n"), *Skill.DisplayName);
		Output += FString::Printf(TEXT("## Description\n%s\n"), *Skill.Description);
		Output += TEXT("## Workflow\n");
		Output += Skill.Content;
		Output += TEXT("\n\n---\n\n");
	}

	return Output;
}

TArray<FString> FAntigravitySkillsManager::GetSkillNames() const
{
	TArray<FString> Names;
	for (const FAntigravitySkill& Skill : Skills)
	{
		Names.Add(Skill.Name);
	}
	return Names;
}

TArray<FAntigravitySkill> FAntigravitySkillsManager::GetSuggestions(const FString& Partial) const
{
	TArray<FAntigravitySkill> Suggestions;
	for (const FAntigravitySkill& Skill : Skills)
	{
		if (Skill.Name.StartsWith(Partial, ESearchCase::IgnoreCase))
		{
			Suggestions.Add(Skill);
		}
	}
	return Suggestions;
}



