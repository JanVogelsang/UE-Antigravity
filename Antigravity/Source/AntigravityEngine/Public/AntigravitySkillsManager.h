// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A single skill document loaded from disk.
 */
struct ANTIGRAVITYENGINE_API FAntigravitySkill
{
	/** Skill name (filename without extension), e.g. "create-actor" */
	FString Name;

	/** Human-readable display name */
	FString DisplayName;

	/** Short description shown in autocomplete */
	FString Description;

	/** Full markdown content of the skill document */
	FString Content;

	/** File path of the skill document */
	FString FilePath;

	/** Tags for categorization */
	TArray<FString> Tags;
};

/**
 * Manages loadable skill documents (step-by-step instruction guides for Antigravity/Gemini).
 *
 * WHAT ARE SKILLS:
 *   Skills are Markdown files stored in Resources/Skills/ that contain detailed,
 *   step-by-step instructions for accomplishing common UE development tasks.
 *
 * CONTEXT CACHING:
 *   With Gemini's massive context window, all loaded skills are concatenated
 *   into a single context string via GetAllSkillsContextString() and injected
 *   into the system prompt upon initialization. This takes advantage of Context
 *   Caching for zero-shot, low-latency execution without needing RAG tool calls.
 *
 * BUILT-IN SKILLS:
 *   create-actor    - Full workflow: C++ header -> source -> Blueprint -> test
 *   setup-input     - Enhanced Input system configuration
 *   create-interface - Blueprint interface creation and usage
 *   add-component   - Add and configure a UE component
 *   setup-replication - Configure actor replication for multiplayer
 *   create-hud      - Create and configure a HUD class
 *   add-save-game   - Add SaveGame system to a project
 *   setup-ai        - Configure AI character with Behavior Tree
 *
 * CUSTOM SKILLS:
 *   Users can create their own .md skill files in Resources/Skills/
 *   and they will be loaded automatically.
 */
class ANTIGRAVITYENGINE_API FAntigravitySkillsManager
{
public:
	FAntigravitySkillsManager();
	~FAntigravitySkillsManager();

	/** Load all skills from Resources/Skills/ directory */
	void Initialize();

	/**
	 * Get the content of a skill by name, with {{arg}} replaced.
	 *
	 * @param SkillName   Skill name (e.g., "create-actor")
	 * @param Args        Optional arguments to substitute for {{arg}}
	 * @param OutContent  The skill content with argument substitution
	 * @return true if skill was found
	 */
	bool GetSkillContent(
		const FString& SkillName,
		const FString& Args,
		FString& OutContent
	) const;

	/**
	 * Concatenates all loaded skills into a single markdown string
	 * optimized for Gemini Context Caching.
	 */
	FString GetAllSkillsContextString() const;

	/** Get all available skills */
	const TArray<FAntigravitySkill>& GetAllSkills() const { return Skills; }

	/** Get skill names for autocomplete */
	TArray<FString> GetSkillNames() const;

	/** Get autocomplete suggestions for a partial skill name */
	TArray<FAntigravitySkill> GetSuggestions(const FString& Partial) const;

	/** Get number of loaded skills */
	int32 GetSkillCount() const { return Skills.Num(); }

	/** Get skills directory path */
	static FString GetSkillsDirectory();


private:
	void LoadSkillFromFile(const FString& FilePath);
	FAntigravitySkill ParseSkillDocument(const FString& Content, const FString& FilePath) const;

	TArray<FAntigravitySkill> Skills;

};
