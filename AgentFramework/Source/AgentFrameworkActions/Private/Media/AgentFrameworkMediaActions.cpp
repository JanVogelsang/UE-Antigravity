// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Media/AgentFrameworkMediaActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IAssetTools.h"

#include "MediaPlayer.h"
#include "FileMediaSource.h"
#include "StreamMediaSource.h"
#include "MediaTexture.h"
#include "MediaSource.h"

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
#include "Factories/SoundCueFactoryNew.h"
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
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	bool bCreateTexture = false;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("create_texture"), bCreateTexture, Result.Errors, false);

	bool bCreateSource = false;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("create_source"), bCreateSource, Result.Errors, false);

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*AssetPath);
	if (!IsValid(Package))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create package for asset at '%s'."), *AssetPath));
		return Result;
	}

	UMediaPlayer* MediaPlayer = NewObject<UMediaPlayer>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!IsValid(MediaPlayer))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Media Player asset at '%s'."), *AssetPath));
		return Result;
	}

	MediaPlayer->Modify();
	FAssetRegistryModule::AssetCreated(MediaPlayer);
	Result.ModifiedAssets.Add(AssetPath);

	if (bCreateTexture)
	{
		FString TexAssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("texture_asset_path"), TexAssetPath, Result.Errors, false) || TexAssetPath.IsEmpty())
		{
			TexAssetPath = AssetPath + TEXT("_VideoTexture");
		}
		FString TexPackagePath = FPackageName::GetLongPackagePath(TexAssetPath);
		FString TexAssetName = FPackageName::GetLongPackageAssetName(TexAssetPath);

		UPackage* TexPackage = CreatePackage(*TexAssetPath);
		if (IsValid(TexPackage))
		{
			UMediaTexture* MediaTex = NewObject<UMediaTexture>(TexPackage, *TexAssetName, RF_Public | RF_Standalone);
			if (IsValid(MediaTex))
			{
				MediaTex->SetMediaPlayer(MediaPlayer);
				MediaTex->UpdateResource();
				FAssetRegistryModule::AssetCreated(MediaTex);
				MediaTex->MarkPackageDirty();
				Result.ModifiedAssets.Add(TexAssetPath);
			}
		}
	}

	if (bCreateSource)
	{
		FString SourceAssetPath;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("source_asset_path"), SourceAssetPath, Result.Errors, false) || SourceAssetPath.IsEmpty())
		{
			SourceAssetPath = AssetPath + TEXT("_Source");
		}
		FString SourcePackagePath = FPackageName::GetLongPackagePath(SourceAssetPath);
		FString SourceAssetName = FPackageName::GetLongPackageAssetName(SourceAssetPath);

		UPackage* SrcPackage = CreatePackage(*SourceAssetPath);
		if (IsValid(SrcPackage))
		{
			UFileMediaSource* FileSource = NewObject<UFileMediaSource>(SrcPackage, *SourceAssetName, RF_Public | RF_Standalone);
			if (IsValid(FileSource))
			{
				FString FilePath;
				if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("file_path"), FilePath, Result.Errors, false) && !FilePath.IsEmpty())
				{
					FileSource->SetFilePath(FilePath);
				}
				FAssetRegistryModule::AssetCreated(FileSource);
				FileSource->MarkPackageDirty();
				Result.ModifiedAssets.Add(SourceAssetPath);
			}
		}
	}

	MediaPlayer->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Successfully created Media Player asset at '%s'."), *AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkMediaActions::ExecuteCreateMediaTexture(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
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

	UMediaTexture* MediaTexture = NewObject<UMediaTexture>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!IsValid(MediaTexture))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Media Texture asset at '%s'."), *AssetPath));
		return Result;
	}

	MediaTexture->Modify();

	FString MediaPlayerPath;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("media_player"), MediaPlayerPath, Result.Errors, false) && !MediaPlayerPath.IsEmpty())
	{
		UMediaPlayer* BoundPlayer = LoadObject<UMediaPlayer>(nullptr, *MediaPlayerPath);
		if (IsValid(BoundPlayer))
		{
			MediaTexture->SetMediaPlayer(BoundPlayer);
		}
	}

	MediaTexture->UpdateResource();
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
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString FilePath;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("file_path"), FilePath, Result.Errors, false);

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*AssetPath);
	if (!IsValid(Package))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create package for asset at '%s'."), *AssetPath));
		return Result;
	}

	UFileMediaSource* MediaSource = NewObject<UFileMediaSource>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!IsValid(MediaSource))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create File Media Source asset at '%s'."), *AssetPath));
		return Result;
	}

	MediaSource->Modify();

	if (!FilePath.IsEmpty())
	{
		MediaSource->SetFilePath(FilePath);
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
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UMediaPlayer* MediaPlayer = LoadObject<UMediaPlayer>(nullptr, *AssetPath);
	if (!IsValid(MediaPlayer))
	{
		Result.Errors.Add(FString::Printf(TEXT("Media Player asset not found at '%s'."), *AssetPath));
		return Result;
	}

	MediaPlayer->Modify();

	bool bLooping = false;
	if (UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("looping"), bLooping, Result.Errors, false))
	{
		if (Params->HasField(TEXT("looping")))
		{
			MediaPlayer->SetLooping(bLooping);
		}
	}

	bool bShuffle = false;
	if (UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("shuffle"), bShuffle, Result.Errors, false))
	{
		if (Params->HasField(TEXT("shuffle")))
		{
			MediaPlayer->Shuffle = bShuffle ? 1 : 0;
		}
	}

	FString MediaSourcePath;
	if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("media_source"), MediaSourcePath, Result.Errors, false) && !MediaSourcePath.IsEmpty())
	{
		UMediaSource* MediaSource = LoadObject<UMediaSource>(nullptr, *MediaSourcePath);
		if (IsValid(MediaSource))
		{
			MediaPlayer->OpenSource(MediaSource);
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Media Source not found at '%s'."), *MediaSourcePath));
		}
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

	if (UMediaPlayer* MediaPlayer = Cast<UMediaPlayer>(Asset))
	{
		InfoObj->SetBoolField(TEXT("is_looping"), MediaPlayer->IsLooping());
		InfoObj->SetBoolField(TEXT("is_playing"), MediaPlayer->IsPlaying());
		InfoObj->SetBoolField(TEXT("is_paused"), MediaPlayer->IsPaused());
		InfoObj->SetStringField(TEXT("url"), MediaPlayer->GetUrl());
	}
	else if (UFileMediaSource* FileSource = Cast<UFileMediaSource>(Asset))
	{
		InfoObj->SetStringField(TEXT("file_path"), FileSource->GetFilePath());
		InfoObj->SetStringField(TEXT("url"), FileSource->GetUrl());
	}
	else if (UMediaTexture* MediaTexture = Cast<UMediaTexture>(Asset))
	{
		InfoObj->SetNumberField(TEXT("width"), MediaTexture->GetWidth());
		InfoObj->SetNumberField(TEXT("height"), MediaTexture->GetHeight());
		InfoObj->SetStringField(TEXT("aspect_ratio"), FString::SanitizeFloat(MediaTexture->GetAspectRatio()));
		if (IsValid(MediaTexture->GetMediaPlayer()))
		{
			InfoObj->SetStringField(TEXT("bound_media_player"), MediaTexture->GetMediaPlayer()->GetPathName());
		}
	}

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
			USoundCueFactoryNew* CueFactory = NewObject<USoundCueFactoryNew>();
			if (IsValid(CueFactory))
			{
				IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
				SoundCue = Cast<USoundCue>(AssetTools.CreateAsset(CueAssetName, CuePackagePath, USoundCue::StaticClass(), CueFactory));
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
