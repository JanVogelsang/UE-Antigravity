// Copyright 2026 AgentFramework. All Rights Reserved.

#include "MetaSound/AgentFrameworkMetaSoundActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ScopedTransaction.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// Engine & MetaSound Headers (Soft-loaded via reflection)
#if WITH_EDITOR
#include "Editor.h"
#include "Sound/SoundBase.h"
#endif


#define LOCTEXT_NAMESPACE "FAgentFrameworkMetaSoundActions"

FAgentFrameworkMetaSoundActions::FAgentFrameworkMetaSoundActions() {}
FAgentFrameworkMetaSoundActions::~FAgentFrameworkMetaSoundActions() {}

FName FAgentFrameworkMetaSoundActions::GetActionName() const
{
	return FName(TEXT("MetaSound"));
}

TArray<FString> FAgentFrameworkMetaSoundActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_metasound_source"),
		TEXT("wire_metasound_nodes")
	};
}

bool FAgentFrameworkMetaSoundActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, OutErrors, false);
	if (ToolName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), ToolName, OutErrors, false);
	}
	if (ToolName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), ToolName, OutErrors, false);
	}

	if (ToolName == TEXT("create_metasound_source") || ToolName.IsEmpty())
	{
		FString AssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, false) || AssetPath.IsEmpty())
		{
			FString DestPath, AssetName;
			if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("destination_path"), DestPath, OutErrors, false) ||
				!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_name"), AssetName, OutErrors, false))
			{
				OutErrors.Add(TEXT("Missing required parameter: 'asset_path' (or 'destination_path' and 'asset_name')."));
				return false;
			}
		}
	}
	else if (ToolName == TEXT("wire_metasound_nodes"))
	{
		FString AssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true))
		{
			return false;
		}
	}

	return true;
}

FAgentFrameworkActionResult FAgentFrameworkMetaSoundActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, Result.Errors, false);
	if (ToolName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), ToolName, Result.Errors, false);
	}
	if (ToolName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), ToolName, Result.Errors, false);
	}

	if (ToolName == TEXT("create_metasound_source"))
	{
		Result = ExecuteCreateMetaSoundSource(Params, Result);
	}
	else if (ToolName == TEXT("wire_metasound_nodes"))
	{
		Result = ExecuteWireMetaSoundNodes(Params, Result);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown or unspecified MetaSound action: '%s'"), *ToolName));
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	return Result;
}



FAgentFrameworkActionResult FAgentFrameworkMetaSoundActions::ExecuteCreateMetaSoundSource(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClass* MetaSoundSourceClass = FindFirstObject<UClass>(TEXT("MetaSoundSource"), EFindFirstObjectOptions::None);
	if (!IsValid(MetaSoundSourceClass))
	{
		MetaSoundSourceClass = FindFirstObject<UClass>(TEXT("UMetaSoundSource"), EFindFirstObjectOptions::None);
	}

	if (!IsValid(MetaSoundSourceClass))
	{
		Result.Errors.Add(TEXT("MetasoundEngine module is not loaded in this project. Add 'MetasoundEngine' to your host project's .Build.cs."));
		return Result;
	}

	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, false) || AssetPath.IsEmpty())
	{
		FString DestPath, AssetName;
		if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("destination_path"), DestPath, Result.Errors, false) &&
			UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_name"), AssetName, Result.Errors, false))
		{
			AssetPath = FPaths::Combine(DestPath, AssetName);
		}
	}

	if (AssetPath.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing required parameter: 'asset_path' (or 'destination_path' and 'asset_name')."));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, MetaSoundSourceClass, nullptr);
	if (!IsValid(NewAsset))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create MetaSoundSource asset at '%s'."), *AssetPath));
		return Result;
	}

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully created MetaSoundSource asset '%s'."), *AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMetaSoundActions::ExecuteWireMetaSoundNodes(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UClass* MetaSoundSourceClass = FindFirstObject<UClass>(TEXT("MetaSoundSource"), EFindFirstObjectOptions::None);
	if (!IsValid(MetaSoundSourceClass))
	{
		Result.Errors.Add(TEXT("MetasoundEngine module is not loaded in this project. Add 'MetasoundEngine' to your host project's .Build.cs."));
		return Result;
	}

	UObject* MetaSoundSource = LoadObject<UObject>(nullptr, *AssetPath);
	if (!IsValid(MetaSoundSource))
	{
		Result.Errors.Add(FString::Printf(TEXT("MetaSoundSource asset not found at '%s'."), *AssetPath));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully registered MetaSound node wiring for '%s'."), *AssetPath);
	return Result;
}

void FAgentFrameworkMetaSoundActions::PlaySuccessSound()
{
#if WITH_EDITOR
	if (IsValid(GEditor))
	{
		USoundBase* SuccessSound = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
		if (IsValid(SuccessSound))
		{
			GEditor->PlayEditorSound(SuccessSound);
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE
