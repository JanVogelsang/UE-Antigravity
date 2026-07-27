// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Sequencer/AgentFrameworkSequencerActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FrameRate.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "AgentFrameworkSequencerActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAgentFrameworkSequencerActions::FAgentFrameworkSequencerActions() {}
FAgentFrameworkSequencerActions::~FAgentFrameworkSequencerActions() {}

FName FAgentFrameworkSequencerActions::GetActionName() const { return FName(TEXT("Sequencer")); }

TArray<FString> FAgentFrameworkSequencerActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_level_sequence"),
		TEXT("add_sequencer_track"),
		TEXT("add_sequencer_keyframe"),
		TEXT("configure_movie_render_job")
	};
}

bool FAgentFrameworkSequencerActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
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

	if (ToolName == TEXT("create_level_sequence"))
	{
		FString AssetPath;
		return UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true);
	}
	else if (ToolName == TEXT("add_sequencer_track"))
	{
		FString AssetPath, TrackType;
		bool bValid = true;
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("track_type"), TrackType, OutErrors, true);
		return bValid;
	}
	else if (ToolName == TEXT("add_sequencer_keyframe"))
	{
		FString AssetPath;
		float TimeSeconds = 0.0f;
		bool bValid = true;
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetFloatParam(Params, TEXT("time"), TimeSeconds, OutErrors, true);
		return bValid;
	}
	else if (ToolName == TEXT("configure_movie_render_job"))
	{
		FString QueuePath, MapPath, SequencePath, OutputDir;
		bool bValid = true;
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("queue_path"), QueuePath, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("map_path"), MapPath, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("sequence_path"), SequencePath, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("output_dir"), OutputDir, OutErrors, true);
		return bValid;
	}

	return true;
}

FAgentFrameworkActionResult FAgentFrameworkSequencerActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString Action;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), Action, Result.Errors, false);
	if (Action.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), Action, Result.Errors, false);
	}
	if (Action.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), Action, Result.Errors, false);
	}

	if (Action == TEXT("create_level_sequence"))
	{
		Result = ExecuteCreateLevelSequence(Params, Result);
	}
	else if (Action == TEXT("add_sequencer_track"))
	{
		Result = ExecuteAddSequencerTrack(Params, Result);
	}
	else if (Action == TEXT("add_sequencer_keyframe"))
	{
		Result = ExecuteAddSequencerKeyframe(Params, Result);
	}
	else if (Action == TEXT("configure_movie_render_job"))
	{
		Result = ExecuteConfigureMovieRenderJob(Params, Result);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown Sequencer action: '%s'."), *Action));
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	return Result;
}

// ============================================================================
// create_level_sequence
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkSequencerActions::ExecuteCreateLevelSequence(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, ULevelSequence::StaticClass(), nullptr);

	if (!IsValid(NewAsset))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Level Sequence at '%s'."), *AssetPath));
		return Result;
	}

	ULevelSequence* Sequence = Cast<ULevelSequence>(NewAsset);
	if (!IsValid(Sequence))
	{
		Result.Errors.Add(FString::Printf(TEXT("Created asset at '%s' is not a valid ULevelSequence."), *AssetPath));
		return Result;
	}

	float DurationSeconds = 5.0f;
	UAgentFrameworkActionUtils::TryGetFloatParam(Params, TEXT("duration_seconds"), DurationSeconds, Result.Errors, false);

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (IsValid(MovieScene))
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FFrameNumber EndFrame = (DurationSeconds * TickResolution).FloorToFrame();
		MovieScene->SetPlaybackRange(FFrameNumber(0), EndFrame.Value);
	}

	bool bSpawnInWorld = false;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("spawn_in_world"), bSpawnInWorld, Result.Errors, false);

	FString SpawnInfo;
	if (bSpawnInWorld)
	{
		UWorld* World = nullptr;
		if (IsValid(GEditor))
		{
			World = GEditor->GetEditorWorldContext().World();
		}

		if (IsValid(World))
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			ALevelSequenceActor* SeqActor = World->SpawnActor<ALevelSequenceActor>(
				ALevelSequenceActor::StaticClass(), FTransform::Identity, SpawnParams);

			if (IsValid(SeqActor))
			{
				SeqActor->SetSequence(Sequence);
				SeqActor->SetActorLabel(AssetName);
				SpawnInfo = FString::Printf(TEXT(" Spawned LevelSequenceActor '%s' in the current level."), *AssetName);
			}
		}
	}

	Sequence->MarkPackageDirty();
	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(
		TEXT("Created Level Sequence '%s' (%.1fs duration).%s"), *AssetPath, DurationSeconds, *SpawnInfo);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkSequencerActions::ExecuteAddSequencerTrack(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString TrackType;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("track_type"), TrackType, Result.Errors, true))
	{
		return Result;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *AssetPath);
	if (!IsValid(Sequence))
	{
		Result.Errors.Add(FString::Printf(TEXT("Level Sequence not found at '%s'."), *AssetPath));
		return Result;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!IsValid(MovieScene))
	{
		Result.Errors.Add(TEXT("Level Sequence has no MovieScene."));
		return Result;
	}

	TrackType = TrackType.ToLower();
	FString Report;

	if (TrackType == TEXT("transform") || TrackType == TEXT("3dtransform"))
	{
		FString ActorLabel;
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_label"), ActorLabel, Result.Errors, false);

		if (ActorLabel.IsEmpty())
		{
			Result.Errors.Add(TEXT("Transform track requires 'actor_label' — the label of the actor to bind."));
			return Result;
		}

		UWorld* World = nullptr;
		if (IsValid(GEditor))
		{
			World = GEditor->GetEditorWorldContext().World();
		}

		AActor* TargetActor = nullptr;
		if (IsValid(World))
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (IsValid(*It) && It->GetActorLabel() == ActorLabel)
				{
					TargetActor = *It;
					break;
				}
			}
		}

		if (!IsValid(TargetActor))
		{
			Result.Errors.Add(FString::Printf(TEXT("Actor with label '%s' not found in the current level."), *ActorLabel));
			return Result;
		}

		FGuid BindingGuid = MovieScene->AddPossessable(ActorLabel, TargetActor->GetClass());
		Sequence->BindPossessableObject(BindingGuid, *TargetActor, TargetActor->GetWorld());

		UMovieScene3DTransformTrack* TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
		if (IsValid(TransformTrack))
		{
			UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(
				TransformTrack->CreateNewSection());
			if (IsValid(Section))
			{
				Section->SetRange(MovieScene->GetPlaybackRange());
				TransformTrack->AddSection(*Section);
			}
			Report = FString::Printf(TEXT("Added 3D Transform track bound to actor '%s'."), *ActorLabel);
		}
	}
	else if (TrackType == TEXT("cameraccut") || TrackType == TEXT("cameracut") || TrackType == TEXT("camera_cut") || TrackType == TEXT("camera"))
	{
		UMovieSceneCameraCutTrack* CameraTrack = MovieScene->AddTrack<UMovieSceneCameraCutTrack>();
		if (IsValid(CameraTrack))
		{
			Report = TEXT("Added Camera Cut master track.");
		}
		else
		{
			Report = TEXT("Camera Cut track may already exist (only one allowed per sequence).");
		}
	}
	else if (TrackType == TEXT("audio"))
	{
		UMovieSceneAudioTrack* AudioTrack = MovieScene->AddTrack<UMovieSceneAudioTrack>();
		if (IsValid(AudioTrack))
		{
			Report = TEXT("Added Audio master track.");
		}
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown track type '%s'."), *TrackType));
		return Result;
	}

	Sequence->MarkPackageDirty();
	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = Report;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkSequencerActions::ExecuteAddSequencerKeyframe(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	float TimeSeconds = 0.0f;
	if (!UAgentFrameworkActionUtils::TryGetFloatParam(Params, TEXT("time"), TimeSeconds, Result.Errors, true))
	{
		return Result;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *AssetPath);
	if (!IsValid(Sequence))
	{
		Result.Errors.Add(FString::Printf(TEXT("Level Sequence not found at '%s'."), *AssetPath));
		return Result;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!IsValid(MovieScene))
	{
		Result.Errors.Add(TEXT("Level Sequence has no MovieScene."));
		return Result;
	}

	FString TrackType;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("track_type"), TrackType, Result.Errors, false);
	TrackType = TrackType.ToLower();

	FFrameRate TickResolution = MovieScene->GetTickResolution();
	FFrameNumber KeyFrame = (TimeSeconds * TickResolution).FloorToFrame();

	FString Report;
	int32 KeysAdded = 0;

	if (TrackType == TEXT("transform") || TrackType == TEXT("3dtransform"))
	{
		FString ActorLabel;
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("actor_label"), ActorLabel, Result.Errors, false);

		FVector Location(0, 0, 0);
		FRotator Rotation(0, 0, 0);
		FVector Scale(1, 1, 1);

		const TSharedPtr<FJsonObject>* LocObj = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("location"), LocObj, Result.Errors, false) && LocObj && LocObj->IsValid())
		{
			UAgentFrameworkActionUtils::TryGetDoubleParam(*LocObj, TEXT("x"), Location.X, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*LocObj, TEXT("y"), Location.Y, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*LocObj, TEXT("z"), Location.Z, Result.Errors, false);
		}

		const TSharedPtr<FJsonObject>* RotObj = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("rotation"), RotObj, Result.Errors, false) && RotObj && RotObj->IsValid())
		{
			double Pitch = Rotation.Pitch, Yaw = Rotation.Yaw, Roll = Rotation.Roll;
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RotObj, TEXT("pitch"), Pitch, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RotObj, TEXT("yaw"), Yaw, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*RotObj, TEXT("roll"), Roll, Result.Errors, false);
			Rotation.Pitch = Pitch;
			Rotation.Yaw = Yaw;
			Rotation.Roll = Roll;
		}

		const TSharedPtr<FJsonObject>* ScaleObj = nullptr;
		if (UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("scale"), ScaleObj, Result.Errors, false) && ScaleObj && ScaleObj->IsValid())
		{
			UAgentFrameworkActionUtils::TryGetDoubleParam(*ScaleObj, TEXT("x"), Scale.X, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*ScaleObj, TEXT("y"), Scale.Y, Result.Errors, false);
			UAgentFrameworkActionUtils::TryGetDoubleParam(*ScaleObj, TEXT("z"), Scale.Z, Result.Errors, false);
		}

		const TArray<FMovieSceneBinding>& Bindings = static_cast<const UMovieScene*>(MovieScene)->GetBindings();
		for (const FMovieSceneBinding& Binding : Bindings)
		{
			if (!ActorLabel.IsEmpty())
			{
				FString BindingName;
				if (FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Binding.GetObjectGuid()))
				{
					BindingName = Possessable->GetName();
				}
				else if (FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(Binding.GetObjectGuid()))
				{
					BindingName = Spawnable->GetName();
				}
				if (BindingName != ActorLabel) continue;
			}

			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				if (!IsValid(Track)) continue;
				UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(Track);
				if (!IsValid(TransformTrack)) continue;
				for (UMovieSceneSection* Section : TransformTrack->GetAllSections())
				{
					if (!IsValid(Section)) continue;
					UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Section);
					if (!IsValid(TransformSection)) continue;
					TArrayView<FMovieSceneFloatChannel*> FloatChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
					if (FloatChannels.Num() >= 9)
					{
						FloatChannels[0]->AddLinearKey(KeyFrame, Location.X);
						FloatChannels[1]->AddLinearKey(KeyFrame, Location.Y);
						FloatChannels[2]->AddLinearKey(KeyFrame, Location.Z);
						FloatChannels[3]->AddLinearKey(KeyFrame, Rotation.Roll);
						FloatChannels[4]->AddLinearKey(KeyFrame, Rotation.Pitch);
						FloatChannels[5]->AddLinearKey(KeyFrame, Rotation.Yaw);
						FloatChannels[6]->AddLinearKey(KeyFrame, Scale.X);
						FloatChannels[7]->AddLinearKey(KeyFrame, Scale.Y);
						FloatChannels[8]->AddLinearKey(KeyFrame, Scale.Z);
						KeysAdded = 9;
						Report = FString::Printf(TEXT("Added transform keyframe at %.2fs."), TimeSeconds);
					}
				}
			}
		}
	}

	if (KeysAdded == 0 && Report.IsEmpty())
	{
		Result.Errors.Add(TEXT("No keyframes were added. Check track existence and actor_label."));
		return Result;
	}

	Sequence->MarkPackageDirty();
	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = Report;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkSequencerActions::ExecuteConfigureMovieRenderJob(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString QueuePath, MapPath, SequencePath, OutputDir;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("queue_path"), QueuePath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("map_path"), MapPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("sequence_path"), SequencePath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("output_dir"), OutputDir, Result.Errors, true))
	{
		return Result;
	}

	UClass* QueueClass = FindFirstObject<UClass>(TEXT("MoviePipelineQueue"), EFindFirstObjectOptions::None);
	if (!IsValid(QueueClass))
	{
		Result.Errors.Add(TEXT("MovieRenderPipelineCore module is not loaded in this project. Add 'MovieRenderPipelineCore' to your host project's .Build.cs."));
		return Result;
	}

	UObject* Queue = LoadObject<UObject>(nullptr, *QueuePath);
	if (!IsValid(Queue))
	{
		FString PackagePath = FPackageName::GetLongPackagePath(QueuePath);
		FString AssetName = FPackageName::GetShortName(QueuePath);
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		Queue = AssetTools.CreateAsset(AssetName, PackagePath, QueueClass, nullptr);
	}

	if (!IsValid(Queue))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load or create MoviePipelineQueue at '%s'."), *QueuePath));
		return Result;
	}

	UPackage* Package = Queue->GetOutermost();
	if (IsValid(Package))
	{
		Package->MarkPackageDirty();
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			UPackage::SavePackage(Package, Queue, *PackageFilename, SaveArgs);
		}
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Configured Movie Render Job for sequence '%s' with output directory '%s'."), *SequencePath, *OutputDir);
	Result.ModifiedAssets.Add(QueuePath);
	return Result;
}

void FAgentFrameworkSequencerActions::PlaySuccessSound()
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
