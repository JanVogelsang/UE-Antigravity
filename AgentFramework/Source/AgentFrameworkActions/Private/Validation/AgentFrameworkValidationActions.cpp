// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Validation/AgentFrameworkValidationActions.h"
#include "AgentFrameworkActionUtils.h"
#include "AgentFrameworkCoreModule.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ObjectRedirector.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorValidatorSubsystem.h"
#include "Misc/AutomationTest.h"

#define LOCTEXT_NAMESPACE "AgentFrameworkValidationActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAgentFrameworkValidationActions::FAgentFrameworkValidationActions() {}
FAgentFrameworkValidationActions::~FAgentFrameworkValidationActions() {}

// ============================================================================
// IAgentFrameworkActionExecutor Interface
// ============================================================================

FName FAgentFrameworkValidationActions::GetActionName() const { return FName(TEXT("Validation")); }

TArray<FString> FAgentFrameworkValidationActions::GetSupportedToolNames() const
{
	return {
		TEXT("validate_assets"),
		TEXT("run_automation_tests"),
		TEXT("validate_naming_conventions"),
		TEXT("validate_redirectors"),
		TEXT("validate_map")
	};
}

bool FAgentFrameworkValidationActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkValidationActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString Action;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), Action, Result.Errors, false) || Action.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), Action, Result.Errors, false);
	}

	if (Action == TEXT("validate_assets"))
	{
		return ExecuteValidateAssets(Params, Result);
	}
	else if (Action == TEXT("run_automation_tests"))
	{
		return ExecuteRunAutomationTests(Params, Result);
	}
	else if (Action == TEXT("validate_naming_conventions"))
	{
		return ExecuteValidateNamingConventions(Params, Result);
	}
	else if (Action == TEXT("validate_redirectors"))
	{
		return ExecuteValidateRedirectors(Params, Result);
	}
	else if (Action == TEXT("validate_map"))
	{
		return ExecuteValidateMap(Params, Result);
	}

	// Infer action based on parameters if unspecified
	if (Params->HasField(TEXT("asset_paths")) || Params->HasField(TEXT("validate_all")))
	{
		return ExecuteValidateAssets(Params, Result);
	}
	if (Params->HasField(TEXT("test_filter")))
	{
		return ExecuteRunAutomationTests(Params, Result);
	}
	if (Params->HasField(TEXT("naming_path")) || Params->HasField(TEXT("check_naming")))
	{
		return ExecuteValidateNamingConventions(Params, Result);
	}
	if (Params->HasField(TEXT("redirector_path")) || Params->HasField(TEXT("check_redirectors")))
	{
		return ExecuteValidateRedirectors(Params, Result);
	}
	if (Params->HasField(TEXT("map_path")) || Params->HasField(TEXT("level_path")))
	{
		return ExecuteValidateMap(Params, Result);
	}

	Result.Errors.Add(TEXT("Could not determine validation action. Use validate_assets, run_automation_tests, validate_naming_conventions, validate_redirectors, or validate_map."));
	return Result;
}

// ============================================================================
// validate_assets
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkValidationActions::ExecuteValidateAssets(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	// Get the EditorValidatorSubsystem safely
	if (!GEditor || !IsValid(GEditor))
	{
		Result.Errors.Add(TEXT("GEditor not available."));
		return Result;
	}

	UEditorValidatorSubsystem* ValidatorSubsystem = GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();
	if (!ValidatorSubsystem || !IsValid(ValidatorSubsystem))
	{
		Result.Errors.Add(TEXT("UEditorValidatorSubsystem is not available or invalid. It may not be enabled in this engine version."));
		return Result;
	}

	// Collect asset paths to validate
	TArray<FAssetData> AssetsToValidate;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	bool bValidateAll = false;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("validate_all"), bValidateAll, Result.Errors, false);

	TArray<FString> AssetPaths;
	UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("asset_paths"), AssetPaths, Result.Errors, false);

	if (AssetPaths.Num() > 0)
	{
		for (const FString& Path : AssetPaths)
		{
			FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(Path));
			if (AssetData.IsValid())
			{
				AssetsToValidate.Add(AssetData);
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Asset not found in registry: '%s'"), *Path));
			}
		}
	}
	else if (bValidateAll)
	{
		// Get all project assets (limit to /Game/ to avoid engine content)
		TArray<FAssetData> AllAssets;
		AssetRegistry.GetAllAssets(AllAssets, true);
		for (const FAssetData& Asset : AllAssets)
		{
			if (Asset.GetObjectPathString().StartsWith(TEXT("/Game/")))
			{
				AssetsToValidate.Add(Asset);
			}
		}
	}
	else
	{
		Result.Errors.Add(TEXT("Specify 'asset_paths' (array of content paths) or 'validate_all': true."));
		return Result;
	}

	if (AssetsToValidate.Num() == 0)
	{
		Result.bSuccess = true;
		Result.ResultMessage = TEXT("No assets to validate (all specified paths were empty or not found).");
		return Result;
	}

	// Run validation
	UE_LOG(LogAgentFramework, Log, TEXT("ValidationActions: Validating %d assets..."), AssetsToValidate.Num());

	FValidateAssetsSettings ValidationSettings;
	ValidationSettings.bSkipExcludedDirectories = true;
	ValidationSettings.bShowIfNoFailures = false;

	FValidateAssetsResults ValidationResults;
	int32 NumValidated = ValidatorSubsystem->ValidateAssetsWithSettings(AssetsToValidate, ValidationSettings, ValidationResults);

	// Build result report
	FString Report = FString::Printf(TEXT("=== Asset Validation Results ===\nAssets checked: %d\n"), NumValidated);

	Report += FString::Printf(TEXT("  Checked: %d\n"), ValidationResults.NumChecked);
	Report += FString::Printf(TEXT("  Requested: %d\n"), ValidationResults.NumRequested);
	Report += FString::Printf(TEXT("  Unable to validate: %d\n"), ValidationResults.NumUnableToValidate);

	Report += TEXT("\nCheck the Message Log (Window > Developer Tools > Message Log) for detailed validation results.\n");
	Report += TEXT("Use read_message_log with category_filter='LogContentValidation' to see details.\n");

	Result.bSuccess = true;
	Result.ResultMessage = Report;

	return Result;
}

// ============================================================================
// run_automation_tests
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkValidationActions::ExecuteRunAutomationTests(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString TestFilter;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("test_filter"), TestFilter, Result.Errors, false) || TestFilter.IsEmpty())
	{
		TestFilter = TEXT("Project."); // Default: run project tests only
	}

	FString TestFlagsStr;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("test_flags"), TestFlagsStr, Result.Errors, false);

	// Get the automation framework
	FAutomationTestFramework& Framework = FAutomationTestFramework::Get();

	// Find matching tests
	TArray<FAutomationTestInfo> TestInfos;
	Framework.GetValidTestNames(TestInfos);

	TArray<FString> MatchingTests;
	for (const FAutomationTestInfo& TestInfo : TestInfos)
	{
		FString TestName = TestInfo.GetDisplayName();
		if (TestName.Contains(TestFilter, ESearchCase::IgnoreCase) ||
			TestInfo.GetTestName().Contains(TestFilter, ESearchCase::IgnoreCase))
		{
			MatchingTests.Add(TestInfo.GetTestName());
		}
	}

	if (MatchingTests.Num() == 0)
	{
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(
			TEXT("No automation tests matching filter '%s'. Available test count: %d. "
				 "Try broader filters like 'Project.', 'Engine.', or 'Functional.' "
				 "To list all tests, use test_filter='*'."),
			*TestFilter, TestInfos.Num());
		return Result;
	}

	// List matching tests (cap display at 50)
	FString Report = FString::Printf(TEXT("=== Automation Tests matching '%s' ===\n"), *TestFilter);
	Report += FString::Printf(TEXT("Found %d matching tests:\n"), MatchingTests.Num());

	int32 DisplayCount = FMath::Min(MatchingTests.Num(), 50);
	for (int32 i = 0; i < DisplayCount; ++i)
	{
		Report += FString::Printf(TEXT("  %d. %s\n"), i + 1, *MatchingTests[i]);
	}
	if (MatchingTests.Num() > 50)
	{
		Report += FString::Printf(TEXT("  ... and %d more\n"), MatchingTests.Num() - 50);
	}

	// Run tests if requested (not just listing)
	bool bListOnly = false;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("list_only"), bListOnly, Result.Errors, false);

	if (!bListOnly)
	{
		if (GEngine && IsValid(GEngine))
		{
			for (const FString& TestName : MatchingTests)
			{
				GEngine->Exec(nullptr, *FString::Printf(TEXT("Automation RunTest %s"), *TestName));
			}
		}
		else
		{
			Result.Errors.Add(TEXT("GEngine is invalid or not available to execute automation tests."));
			return Result;
		}

		Report += FString::Printf(TEXT("\nQueued %d tests for execution. Tests run asynchronously over multiple frames.\n"), MatchingTests.Num());
		Report += TEXT("Results will appear in:\n");
		Report += TEXT("  1. Window > Developer Tools > Session Frontend > Automation tab\n");
		Report += TEXT("  2. Output Log (use read_message_log with category_filter='LogAutomationTest')\n");
		Report += TEXT("\nCall read_message_log after a few seconds to check test results.\n");
	}

	Result.bSuccess = true;
	Result.ResultMessage = Report;
	return Result;
}

// ============================================================================
// validate_naming_conventions
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkValidationActions::ExecuteValidateNamingConventions(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString SearchPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("path"), SearchPath, Result.Errors, false) || SearchPath.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("search_path"), SearchPath, Result.Errors, false);
	}
	if (SearchPath.IsEmpty())
	{
		SearchPath = TEXT("/Game/");
	}

	TArray<FString> SpecifiedAssetPaths;
	UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("asset_paths"), SpecifiedAssetPaths, Result.Errors, false);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetsToCheck;
	if (SpecifiedAssetPaths.Num() > 0)
	{
		for (const FString& AssetPath : SpecifiedAssetPaths)
		{
			FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
			if (AssetData.IsValid())
			{
				AssetsToCheck.Add(AssetData);
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Asset not found in registry: '%s'"), *AssetPath));
			}
		}
	}
	else
	{
		TArray<FAssetData> AllAssets;
		AssetRegistry.GetAllAssets(AllAssets, true);
		for (const FAssetData& AssetData : AllAssets)
		{
			if (AssetData.GetObjectPathString().StartsWith(SearchPath))
			{
				AssetsToCheck.Add(AssetData);
			}
		}
	}

	auto GetExpectedPrefixes = [](const FName& ClassName) -> TArray<FString>
	{
		FString ClassStr = ClassName.ToString();
		if (ClassStr == TEXT("Blueprint")) return { TEXT("BP_") };
		if (ClassStr == TEXT("WidgetBlueprint")) return { TEXT("WBP_") };
		if (ClassStr == TEXT("Material")) return { TEXT("M_") };
		if (ClassStr == TEXT("MaterialInstanceConstant") || ClassStr == TEXT("MaterialInstance")) return { TEXT("MI_"), TEXT("M_") };
		if (ClassStr == TEXT("MaterialFunction") || ClassStr == TEXT("MaterialFunctionMaterialLayer")) return { TEXT("MF_") };
		if (ClassStr == TEXT("Texture2D") || ClassStr == TEXT("Texture") || ClassStr == TEXT("TextureCube")) return { TEXT("T_") };
		if (ClassStr == TEXT("StaticMesh")) return { TEXT("SM_") };
		if (ClassStr == TEXT("SkeletalMesh")) return { TEXT("SK_"), TEXT("SKM_") };
		if (ClassStr == TEXT("PhysicsAsset")) return { TEXT("PHYS_"), TEXT("PA_") };
		if (ClassStr == TEXT("AnimSequence")) return { TEXT("A_"), TEXT("Anim_") };
		if (ClassStr == TEXT("AnimInstance") || ClassStr == TEXT("AnimBlueprint")) return { TEXT("ABP_") };
		if (ClassStr == TEXT("SoundCue")) return { TEXT("SC_") };
		if (ClassStr == TEXT("SoundWave")) return { TEXT("SW_"), TEXT("S_") };
		if (ClassStr == TEXT("NiagaraSystem")) return { TEXT("NS_") };
		if (ClassStr == TEXT("NiagaraEmitter")) return { TEXT("NE_") };
		if (ClassStr == TEXT("BehaviorTree")) return { TEXT("BT_") };
		if (ClassStr == TEXT("BlackboardData")) return { TEXT("BB_") };
		if (ClassStr == TEXT("UserDefinedStruct")) return { TEXT("F_"), TEXT("F"), TEXT("Struct_") };
		if (ClassStr == TEXT("UserDefinedEnum")) return { TEXT("E_"), TEXT("E"), TEXT("Enum_") };
		if (ClassStr == TEXT("PrimaryDataAsset") || ClassStr == TEXT("DataAsset")) return { TEXT("DA_") };
		if (ClassStr == TEXT("DataTable")) return { TEXT("DT_") };
		return {};
	};

	int32 TotalChecked = 0;
	int32 TotalCompliant = 0;
	int32 TotalViolations = 0;

	FString Report = FString::Printf(TEXT("=== Naming Convention Validation Report ===\nSearch Path / Target: '%s'\nTotal Assets Evaluated: %d\n\n"), *SearchPath, AssetsToCheck.Num());

	TArray<FString> ViolationDetails;

	for (const FAssetData& AssetData : AssetsToCheck)
	{
		FName ClassName = AssetData.AssetClassPath.GetAssetName();
		TArray<FString> ExpectedPrefixes = GetExpectedPrefixes(ClassName);
		if (ExpectedPrefixes.Num() == 0)
		{
			continue;
		}

		TotalChecked++;
		FString AssetName = AssetData.AssetName.ToString();
		bool bCompliant = false;
		for (const FString& Prefix : ExpectedPrefixes)
		{
			if (AssetName.StartsWith(Prefix))
			{
				bCompliant = true;
				break;
			}
		}

		if (bCompliant)
		{
			TotalCompliant++;
		}
		else
		{
			TotalViolations++;
			FString ExpectedStr = FString::Join(ExpectedPrefixes, TEXT(" or "));
			ViolationDetails.Add(FString::Printf(TEXT("  [Violation] %s (%s) — expected prefix '%s', actual name '%s'"),
				*AssetData.GetObjectPathString(), *ClassName.ToString(), *ExpectedStr, *AssetName));
		}
	}

	Report += FString::Printf(TEXT("Rules Applied Assets: %d\n  Compliant: %d\n  Violations: %d\n\n"), TotalChecked, TotalCompliant, TotalViolations);

	if (TotalViolations > 0)
	{
		Report += TEXT("Non-compliant assets:\n");
		int32 DisplayCount = FMath::Min(ViolationDetails.Num(), 50);
		for (int32 i = 0; i < DisplayCount; ++i)
		{
			Report += ViolationDetails[i] + TEXT("\n");
		}
		if (ViolationDetails.Num() > 50)
		{
			Report += FString::Printf(TEXT("  ... and %d more violations.\n"), ViolationDetails.Num() - 50);
		}
	}
	else
	{
		Report += TEXT("All assets with defined naming rules comply with project conventions!\n");
	}

	Result.bSuccess = true;
	Result.ResultMessage = Report;
	return Result;
}

// ============================================================================
// validate_redirectors
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkValidationActions::ExecuteValidateRedirectors(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString SearchPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("path"), SearchPath, Result.Errors, false) || SearchPath.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("search_path"), SearchPath, Result.Errors, false);
	}
	if (SearchPath.IsEmpty())
	{
		SearchPath = TEXT("/Game/");
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAllAssets(AllAssets, true);

	TArray<FAssetData> Redirectors;
	for (const FAssetData& AssetData : AllAssets)
	{
		if (AssetData.GetObjectPathString().StartsWith(SearchPath))
		{
			FName ClassName = AssetData.AssetClassPath.GetAssetName();
			if (ClassName == TEXT("ObjectRedirector") || AssetData.IsRedirector())
			{
				Redirectors.Add(AssetData);
			}
		}
	}

	FString Report = FString::Printf(TEXT("=== Object Redirector Report ===\nSearch Path: '%s'\nRedirectors Found: %d\n\n"), *SearchPath, Redirectors.Num());

	if (Redirectors.Num() == 0)
	{
		Report += TEXT("No ObjectRedirectors found under specified path.\n");
	}
	else
	{
		Report += TEXT("Found Redirectors:\n");
		for (int32 i = 0; i < Redirectors.Num(); ++i)
		{
			const FAssetData& RedirData = Redirectors[i];
			UObjectRedirector* RedirectorObj = Cast<UObjectRedirector>(RedirData.GetAsset());
			if (IsValid(RedirectorObj) && IsValid(RedirectorObj->DestinationObject))
			{
				Report += FString::Printf(TEXT("  %d. %s -> %s (Valid)\n"), i + 1,
					*RedirData.GetObjectPathString(), *RedirectorObj->DestinationObject->GetPathName());
			}
			else
			{
				Report += FString::Printf(TEXT("  %d. %s (Broken / Unresolved Target)\n"), i + 1,
					*RedirData.GetObjectPathString());
			}
		}
		Report += TEXT("\nTip: Use Fixup Redirectors in Content Browser or AssetTools to clean up redirectors.\n");
	}

	Result.bSuccess = true;
	Result.ResultMessage = Report;
	return Result;
}

// ============================================================================
// validate_map
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkValidationActions::ExecuteValidateMap(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!GEditor || !IsValid(GEditor))
	{
		Result.Errors.Add(TEXT("GEditor not available or invalid."));
		return Result;
	}

	FString MapPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("map_path"), MapPath, Result.Errors, false) || MapPath.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("level_path"), MapPath, Result.Errors, false);
	}

	UWorld* TargetWorld = nullptr;
	if (!MapPath.IsEmpty())
	{
		TargetWorld = LoadObject<UWorld>(nullptr, *MapPath);
		if (!IsValid(TargetWorld))
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to load map/level asset at path: '%s'"), *MapPath));
			return Result;
		}
	}
	else
	{
		TargetWorld = GEditor->GetEditorWorldContext().World();
		if (!IsValid(TargetWorld))
		{
			Result.Errors.Add(TEXT("No active editor world found. Specify 'map_path' to validate a specific map asset."));
			return Result;
		}
	}

	FString Report = FString::Printf(TEXT("=== Map / Level Validation Report ===\nMap: '%s'\n"), *TargetWorld->GetPathName());

	// WorldSettings check
	AWorldSettings* WorldSettings = TargetWorld->GetWorldSettings();
	if (IsValid(WorldSettings))
	{
		Report += FString::Printf(TEXT("World Settings: Valid (%s)\n"), *WorldSettings->GetName());
	}
	else
	{
		Report += TEXT("World Settings: MISSING or INVALID!\n");
	}

	ULevel* PersistentLevel = TargetWorld->PersistentLevel;
	if (!IsValid(PersistentLevel))
	{
		Result.Errors.Add(TEXT("PersistentLevel is invalid or null."));
		return Result;
	}

	int32 TotalActors = 0;
	int32 NullSlots = 0;
	int32 MissingRootCount = 0;
	int32 NaNTransformCount = 0;
	TSet<FString> ActorNames;
	int32 DuplicateNamesCount = 0;

	for (AActor* Actor : PersistentLevel->Actors)
	{
		TotalActors++;
		if (!IsValid(Actor))
		{
			NullSlots++;
			continue;
		}

		FString ActorName = Actor->GetActorLabel();
		if (ActorName.IsEmpty())
		{
			ActorName = Actor->GetName();
		}

		if (ActorNames.Contains(ActorName))
		{
			DuplicateNamesCount++;
		}
		else
		{
			ActorNames.Add(ActorName);
		}

		USceneComponent* RootComp = Actor->GetRootComponent();
		if (!IsValid(RootComp))
		{
			MissingRootCount++;
		}
		else
		{
			FVector Loc = RootComp->GetComponentLocation();
			if (Loc.ContainsNaN())
			{
				NaNTransformCount++;
			}
		}
	}

	Report += FString::Printf(TEXT("Actors in Persistent Level: %d\n"), TotalActors);
	Report += FString::Printf(TEXT("  Valid Actors: %d\n"), TotalActors - NullSlots);
	Report += FString::Printf(TEXT("  Null/PendingKill Slots: %d\n"), NullSlots);
	Report += FString::Printf(TEXT("  Actors Missing Root Component: %d\n"), MissingRootCount);
	Report += FString::Printf(TEXT("  Actors with Invalid/NaN Transform: %d\n"), NaNTransformCount);
	Report += FString::Printf(TEXT("  Duplicate Actor Label Count: %d\n"), DuplicateNamesCount);

	if (NullSlots == 0 && MissingRootCount == 0 && NaNTransformCount == 0)
	{
		Report += TEXT("\nMap structure validation passed cleanly with no critical warnings.\n");
	}
	else
	{
		Report += TEXT("\nMap structure issues detected — see breakdown above.\n");
	}

	Result.bSuccess = true;
	Result.ResultMessage = Report;
	return Result;
}

#undef LOCTEXT_NAMESPACE
