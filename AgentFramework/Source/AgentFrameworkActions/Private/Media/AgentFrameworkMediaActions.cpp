// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Media/AgentFrameworkMediaActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IAssetTools.h"

#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundAttenuation.h"
#include "Editor.h"


#define LOCTEXT_NAMESPACE "FAgentFrameworkMediaActions"

FAgentFrameworkMediaActions::FAgentFrameworkMediaActions() {}
FAgentFrameworkMediaActions::~FAgentFrameworkMediaActions() {}

FName FAgentFrameworkMediaActions::GetActionName() const { return FName(TEXT("Media")); }

TArray<FString> FAgentFrameworkMediaActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_media_player"),
		TEXT("create_media_texture"),
		TEXT("create_file_media_source"),
		TEXT("configure_media_player"),
		TEXT("get_media_info"),
		TEXT("configure_sound_wave_cue")
	};
}

bool FAgentFrameworkMediaActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
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

	if (ToolName == TEXT("create_media_player") ||
		ToolName == TEXT("create_media_texture") ||
		ToolName == TEXT("create_file_media_source") ||
		ToolName == TEXT("configure_media_player") ||
		ToolName == TEXT("get_media_info"))
	{
		FString AssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true))
		{
			return false;
		}
	}
	else if (ToolName == TEXT("configure_sound_wave_cue"))
	{
		FString AssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("SoundWaveAsset"), AssetPath, OutErrors, false) || AssetPath.IsEmpty())
		{
			if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true))
			{
				return false;
			}
		}
	}

	return true;
}

FAgentFrameworkActionResult FAgentFrameworkMediaActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
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

	bool bIsReadOnly = (ToolName == TEXT("get_media_info"));
	TOptional<FScopedTransaction> Transaction;
	if (!bIsReadOnly)
	{
		Transaction.Emplace(FText::FromString(TEXT("AgentFramework Media Action")));
	}

	if (ToolName == TEXT("create_media_player"))
	{
		Result = ExecuteCreateMediaPlayer(Params, Result);
	}
	else if (ToolName == TEXT("create_media_texture"))
	{
		Result = ExecuteCreateMediaTexture(Params, Result);
	}
	else if (ToolName == TEXT("create_file_media_source"))
	{
		Result = ExecuteCreateFileMediaSource(Params, Result);
	}
	else if (ToolName == TEXT("configure_media_player"))
	{
		Result = ExecuteConfigureMediaPlayer(Params, Result);
	}
	else if (ToolName == TEXT("get_media_info"))
	{
		Result = ExecuteGetMediaInfo(Params, Result);
	}
	else if (ToolName == TEXT("configure_sound_wave_cue"))
	{
		Result = ExecuteConfigureSoundWaveCue(Params, Result);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown or unsupported Media tool: '%s'"), *ToolName));
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
		OnMediaActionCompleted.Broadcast(FName(*ToolName), Result);
	}
	else if (Transaction.IsSet())
	{
		Transaction->Cancel();
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMediaActions::ExecuteCreateMediaPlayer(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClass* PlayerClass = FindFirstObject<UClass>(TEXT("MediaPlayer"), EFindFirstObjectOptions::None);
	if (!IsValid(PlayerClass))
	{
		Result.Errors.Add(TEXT("MediaAssets module is not loaded in this project. Add 'MediaAssets' to your host project's .Build.cs."));
		return Result;
	}

	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*AssetPath);
	if (!IsValid(Package))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create package for asset at '%s'."), *AssetPath));
		return Result;
	}

	UObject* MediaPlayer = NewObject<UObject>(Package, PlayerClass, *AssetName, RF_Public | RF_Standalone);
	if (!IsValid(MediaPlayer))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Media Player asset at '%s'."), *AssetPath));
		return Result;
	}

	FAssetRegistryModule::AssetCreated(MediaPlayer);
	Result.ModifiedAssets.Add(AssetPath);
	MediaPlayer->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully created Media Player asset at '%s'."), *AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMediaActions::ExecuteCreateMediaTexture(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClass* TexClass = FindFirstObject<UClass>(TEXT("MediaTexture"), EFindFirstObjectOptions::None);
	if (!IsValid(TexClass))
	{
		Result.Errors.Add(TEXT("MediaAssets module is not loaded in this project. Add 'MediaAssets' to your host project's .Build.cs."));
		return Result;
	}

	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*AssetPath);
	if (!IsValid(Package))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create package for asset at '%s'."), *AssetPath));
		return Result;
	}

	UObject* MediaTexture = NewObject<UObject>(Package, TexClass, *AssetName, RF_Public | RF_Standalone);
	if (!IsValid(MediaTexture))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Media Texture asset at '%s'."), *AssetPath));
		return Result;
	}

	FAssetRegistryModule::AssetCreated(MediaTexture);
	MediaTexture->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully created Media Texture asset at '%s'."), *AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMediaActions::ExecuteCreateFileMediaSource(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClass* SourceClass = FindFirstObject<UClass>(TEXT("FileMediaSource"), EFindFirstObjectOptions::None);
	if (!IsValid(SourceClass))
	{
		Result.Errors.Add(TEXT("MediaAssets module is not loaded in this project. Add 'MediaAssets' to your host project's .Build.cs."));
		return Result;
	}

	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*AssetPath);
	if (!IsValid(Package))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create package for asset at '%s'."), *AssetPath));
		return Result;
	}

	UObject* MediaSource = NewObject<UObject>(Package, SourceClass, *AssetName, RF_Public | RF_Standalone);
	if (!IsValid(MediaSource))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create File Media Source asset at '%s'."), *AssetPath));
		return Result;
	}

	FAssetRegistryModule::AssetCreated(MediaSource);
	MediaSource->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully created File Media Source asset at '%s'."), *AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMediaActions::ExecuteConfigureMediaPlayer(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	UClass* PlayerClass = FindFirstObject<UClass>(TEXT("MediaPlayer"), EFindFirstObjectOptions::None);
	if (!IsValid(PlayerClass))
	{
		Result.Errors.Add(TEXT("MediaAssets module is not loaded in this project. Add 'MediaAssets' to your host project's .Build.cs."));
		return Result;
	}

	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UObject* MediaPlayer = LoadObject<UObject>(nullptr, *AssetPath);
	if (!IsValid(MediaPlayer))
	{
		Result.Errors.Add(FString::Printf(TEXT("Media Player asset not found at '%s'."), *AssetPath));
		return Result;
	}

	MediaPlayer->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully configured Media Player asset at '%s'."), *AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMediaActions::ExecuteGetMediaInfo(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!IsValid(Asset))
	{
		Result.Errors.Add(FString::Printf(TEXT("Asset not found at '%s'."), *AssetPath));
		return Result;
	}

	TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
	InfoObj->SetStringField(TEXT("asset_path"), AssetPath);
	InfoObj->SetStringField(TEXT("class_name"), Asset->GetClass()->GetName());

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(InfoObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseString;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMediaActions::ExecuteConfigureSoundWaveCue(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString SoundWaveAssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("SoundWaveAsset"), SoundWaveAssetPath, Result.Errors, false) || SoundWaveAssetPath.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), SoundWaveAssetPath, Result.Errors, false);
	}

	if (SoundWaveAssetPath.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing required parameter 'SoundWaveAsset'."));
		return Result;
	}

	USoundWave* SoundWave = LoadObject<USoundWave>(nullptr, *SoundWaveAssetPath);
	if (!IsValid(SoundWave))
	{
		Result.Errors.Add(FString::Printf(TEXT("SoundWave asset not found at '%s'."), *SoundWaveAssetPath));
		return Result;
	}

	SoundWave->Modify();

	bool bLooping = false;
	if (Params->HasField(TEXT("bLooping")))
	{
		UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("bLooping"), bLooping, Result.Errors, false);
		SoundWave->bLooping = bLooping;
	}
	else if (Params->HasField(TEXT("looping")))
	{
		UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("looping"), bLooping, Result.Errors, false);
		SoundWave->bLooping = bLooping;
	}

	float VolumeMultiplier = 1.0f;
	if (Params->HasField(TEXT("VolumeMultiplier")))
	{
		UAgentFrameworkActionUtils::TryGetFloatParam(Params, TEXT("VolumeMultiplier"), VolumeMultiplier, Result.Errors, false);
		SoundWave->Volume = VolumeMultiplier;
	}
	else if (Params->HasField(TEXT("volume")))
	{
		UAgentFrameworkActionUtils::TryGetFloatParam(Params, TEXT("volume"), VolumeMultiplier, Result.Errors, false);
		SoundWave->Volume = VolumeMultiplier;
	}

	float PitchMultiplier = 1.0f;
	if (Params->HasField(TEXT("PitchMultiplier")))
	{
		UAgentFrameworkActionUtils::TryGetFloatParam(Params, TEXT("PitchMultiplier"), PitchMultiplier, Result.Errors, false);
		SoundWave->Pitch = PitchMultiplier;
	}
	else if (Params->HasField(TEXT("pitch")))
	{
		UAgentFrameworkActionUtils::TryGetFloatParam(Params, TEXT("pitch"), PitchMultiplier, Result.Errors, false);
		SoundWave->Pitch = PitchMultiplier;
	}

	SoundWave->MarkPackageDirty();
	Result.ModifiedAssets.Add(SoundWaveAssetPath);

	FString CueAssetPath;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("CueAssetPath"), CueAssetPath, Result.Errors, false);
	if (CueAssetPath.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("cue_asset_path"), CueAssetPath, Result.Errors, false);
	}

	if (!CueAssetPath.IsEmpty())
	{
		FString CuePackagePath = FPackageName::GetLongPackagePath(CueAssetPath);
		FString CueAssetName = FPackageName::GetLongPackageAssetName(CueAssetPath);

		UPackage* CuePackage = CreatePackage(*CueAssetPath);
		USoundCue* SoundCue = nullptr;

		if (IsValid(CuePackage))
		{
			UClass* CueFactoryClass = FindFirstObject<UClass>(TEXT("SoundCueFactoryNew"), EFindFirstObjectOptions::None);
			if (IsValid(CueFactoryClass))
			{
				UFactory* CueFactory = NewObject<UFactory>(GetTransientPackage(), CueFactoryClass);
				if (IsValid(CueFactory))
				{
					IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
					SoundCue = Cast<USoundCue>(AssetTools.CreateAsset(CueAssetName, CuePackagePath, USoundCue::StaticClass(), CueFactory));
				}
			}

			if (!IsValid(SoundCue))
			{
				SoundCue = NewObject<USoundCue>(CuePackage, *CueAssetName, RF_Public | RF_Standalone);
			}
		}

		if (IsValid(SoundCue))
		{
			SoundCue->Modify();
			USoundNodeWavePlayer* WavePlayerNode = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();
			if (!IsValid(WavePlayerNode))
			{
				WavePlayerNode = NewObject<USoundNodeWavePlayer>(SoundCue);
				SoundCue->AllNodes.Add(WavePlayerNode);
			}

			if (IsValid(WavePlayerNode))
			{
				WavePlayerNode->SetSoundWave(SoundWave);
				SoundCue->FirstNode = WavePlayerNode;
			}

			FString AttenuationAssetPath;
			UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("AttenuationAssetPath"), AttenuationAssetPath, Result.Errors, false);
			if (AttenuationAssetPath.IsEmpty())
			{
				UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("attenuation_asset_path"), AttenuationAssetPath, Result.Errors, false);
			}

			if (!AttenuationAssetPath.IsEmpty())
			{
				USoundAttenuation* AttenuationAsset = LoadObject<USoundAttenuation>(nullptr, *AttenuationAssetPath);
				if (IsValid(AttenuationAsset))
				{
					SoundCue->AttenuationSettings = AttenuationAsset;
				}
				else
				{
					Result.Warnings.Add(FString::Printf(TEXT("USoundAttenuation asset not found at '%s'."), *AttenuationAssetPath));
				}
			}

			SoundCue->PostEditChange();
			SoundCue->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(SoundCue);
			Result.ModifiedAssets.Add(CueAssetPath);
		}
		else
		{
			Result.Errors.Add(FString::Printf(TEXT("Failed to create USoundCue asset at '%s'."), *CueAssetPath));
			return Result;
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully configured SoundWave asset '%s'%s."),
		*SoundWaveAssetPath,
		!CueAssetPath.IsEmpty() ? *FString::Printf(TEXT(" and created SoundCue at '%s'"), *CueAssetPath) : TEXT(""));
	return Result;
}


void FAgentFrameworkMediaActions::PlaySuccessSound()
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
