// Copyright 2026 Antigravity. All Rights Reserved.

#include "Context/AntigravityContextActions.h"
#include "AntigravityCoreModule.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"



FAntigravityContextActions::FAntigravityContextActions() {}
FAntigravityContextActions::~FAntigravityContextActions() {}

FName FAntigravityContextActions::GetActionName() const { return FName(TEXT("Context")); }
FText FAntigravityContextActions::GetDisplayName() const { return FText::FromString(TEXT("Context Exploration")); }
EAntigravityActionCategory FAntigravityContextActions::GetCategory() const { return EAntigravityActionCategory::General; }
EAntigravityRiskLevel FAntigravityContextActions::GetDefaultRiskLevel() const { return EAntigravityRiskLevel::Low; }
bool FAntigravityContextActions::CanUndo() const { return false; }
bool FAntigravityContextActions::UndoAction() { return false; }

TArray<FString> FAntigravityContextActions::GetSupportedToolNames() const
{
	return {
		TEXT("search_assets")
	};
}

bool FAntigravityContextActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAntigravityActionPlan FAntigravityContextActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionPlan Plan;
	Plan.Summary = TEXT("Context exploration (read-only)");
	FAntigravityAction Action;
	Action.Description = Plan.Summary;
	Action.Category = EAntigravityActionCategory::General;
	Action.RiskLevel = EAntigravityRiskLevel::Low;
	Plan.Actions.Add(Action);
	return Plan;
}

FAntigravityActionResult FAntigravityContextActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionResult Result;
	Result.bSuccess = false;

	// Determine action from "action" field, or fall back to "tool_name" (injected by the client)
	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty())
	{
		Params->TryGetStringField(TEXT("tool_name"), Action);
	}

	if (Action == TEXT("search_assets") || Params->HasField(TEXT("query")) || Params->HasField(TEXT("class_filter")))
	{
		return ExecuteSearchAssets(Params, Result);
	}

	Result.Errors.Add(TEXT("Could not determine context action. Use 'action' field or provide 'query'/'class_filter'."));
	return Result;
}



// ============================================================================
// search_assets: Search the asset registry by class, name, or path
// ============================================================================

FAntigravityActionResult FAntigravityContextActions::ExecuteSearchAssets(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
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


