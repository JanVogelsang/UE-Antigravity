// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Sequencer/AgentFrameworkSequencerActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkSettings.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FrameRate.h"

#define LOCTEXT_NAMESPACE "AgentFrameworkSequencerActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAgentFrameworkSequencerActions::FAgentFrameworkSequencerActions() {}
FAgentFrameworkSequencerActions::~FAgentFrameworkSequencerActions() {}

// ============================================================================
// IAgentFrameworkActionExecutor Interface
// ============================================================================

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
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkSequencerActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty())
		Params->TryGetStringField(TEXT("tool_name"), Action);

	if (Action == TEXT("create_level_sequence"))
		return ExecuteCreateLevelSequence(Params, Result);
	else if (Action == TEXT("add_sequencer_track"))
		return ExecuteAddSequencerTrack(Params, Result);
	else if (Action == TEXT("add_sequencer_keyframe"))
		return ExecuteAddSequencerKeyframe(Params, Result);
	else if (Action == TEXT("configure_movie_render_job"))
		return ExecuteConfigureMovieRenderJob(Params, Result);

	Result.Errors.Add(TEXT("Unknown Sequencer action."));
	return Result;
}

// ============================================================================
// create_level_sequence
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkSequencerActions::ExecuteCreateLevelSequence(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Result.Errors.Add(TEXT("Missing required field: 'asset_path'"));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, ULevelSequence::StaticClass(), nullptr);

	if (!NewAsset)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Level Sequence at '%s'."), *AssetPath));
		return Result;
	}

	ULevelSequence* Sequence = Cast<ULevelSequence>(NewAsset);

	// Set duration if specified
	float DurationSeconds = 5.0f;
	Params->TryGetNumberField(TEXT("duration_seconds"), DurationSeconds);

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FFrameNumber EndFrame = (DurationSeconds * TickResolution).FloorToFrame();
		MovieScene->SetPlaybackRange(FFrameNumber(0), EndFrame.Value);
	}

	// Optionally spawn a LevelSequenceActor in the world
	bool bSpawnInWorld = false;
	Params->TryGetBoolField(TEXT("spawn_in_world"), bSpawnInWorld);

	FString SpawnInfo;
	if (bSpawnInWorld)
	{
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (World)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			ALevelSequenceActor* SeqActor = World->SpawnActor<ALevelSequenceActor>(
				ALevelSequenceActor::StaticClass(), FTransform::Identity, SpawnParams);

			if (SeqActor)
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
		TEXT("Created Level Sequence '%s' (%.1fs duration).%s "
			 "Use add_sequencer_track to add actor bindings, camera cuts, or audio tracks."),
		*AssetPath, DurationSeconds, *SpawnInfo);
	return Result;
}

// ============================================================================
// add_sequencer_track
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkSequencerActions::ExecuteAddSequencerTrack(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Result.Errors.Add(TEXT("Missing required field: 'asset_path'"));
		return Result;
	}

	FString TrackType;
	if (!Params->TryGetStringField(TEXT("track_type"), TrackType))
	{
		Result.Errors.Add(TEXT("Missing required field: 'track_type' (Transform, Float, CameraCut, Audio)"));
		return Result;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *AssetPath);
	if (!Sequence)
	{
		Result.Errors.Add(FString::Printf(TEXT("Level Sequence not found at '%s'."), *AssetPath));
		return Result;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		Result.Errors.Add(TEXT("Level Sequence has no MovieScene."));
		return Result;
	}

	TrackType = TrackType.ToLower();
	FString Report;

	if (TrackType == TEXT("transform") || TrackType == TEXT("3dtransform"))
	{
		// Need an actor binding
		FString ActorLabel;
		Params->TryGetStringField(TEXT("actor_label"), ActorLabel);

		if (ActorLabel.IsEmpty())
		{
			Result.Errors.Add(TEXT("Transform track requires 'actor_label' â€” the label of the actor to bind."));
			return Result;
		}

		// Find the actor in the world
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		AActor* TargetActor = nullptr;
		if (World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetActorLabel() == ActorLabel)
				{
					TargetActor = *It;
					break;
				}
			}
		}

		if (!TargetActor)
		{
			Result.Errors.Add(FString::Printf(TEXT("Actor with label '%s' not found in the current level."), *ActorLabel));
			return Result;
		}

		// Create or find the binding
		FGuid BindingGuid = MovieScene->AddPossessable(ActorLabel, TargetActor->GetClass());
		Sequence->BindPossessableObject(BindingGuid, *TargetActor, TargetActor->GetWorld());

		// Add 3D Transform track
		UMovieScene3DTransformTrack* TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(BindingGuid);
		if (TransformTrack)
		{
			UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(
				TransformTrack->CreateNewSection());
			if (Section)
			{
				Section->SetRange(MovieScene->GetPlaybackRange());
				TransformTrack->AddSection(*Section);
			}
			Report = FString::Printf(TEXT("Added 3D Transform track bound to actor '%s'."), *ActorLabel);
		}
	}
	else if (TrackType == TEXT("cameraccut") || TrackType == TEXT("camera_cut") || TrackType == TEXT("camera"))
	{
		UMovieSceneCameraCutTrack* CameraTrack = MovieScene->AddTrack<UMovieSceneCameraCutTrack>();
		if (CameraTrack)
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
		if (AudioTrack)
		{
			Report = TEXT("Added Audio master track.");
		}
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown track type '%s'. Supported: Transform, CameraCut, Audio."), *TrackType));
		return Result;
	}

	Sequence->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = Report;
	return Result;
}

// ============================================================================
// add_sequencer_keyframe
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkSequencerActions::ExecuteAddSequencerKeyframe(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Result.Errors.Add(TEXT("Missing required field: 'asset_path'"));
		return Result;
	}

	float TimeSeconds = 0.0f;
	if (!Params->TryGetNumberField(TEXT("time"), TimeSeconds))
	{
		Result.Errors.Add(TEXT("Missing required field: 'time' (in seconds)"));
		return Result;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *AssetPath);
	if (!Sequence)
	{
		Result.Errors.Add(FString::Printf(TEXT("Level Sequence not found at '%s'."), *AssetPath));
		return Result;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		Result.Errors.Add(TEXT("Level Sequence has no MovieScene."));
		return Result;
	}

	FString TrackType;
	Params->TryGetStringField(TEXT("track_type"), TrackType);
	TrackType = TrackType.ToLower();

	FFrameRate TickResolution = MovieScene->GetTickResolution();
	FFrameNumber KeyFrame = (TimeSeconds * TickResolution).FloorToFrame();

	FString Report;
	int32 KeysAdded = 0;

	if (TrackType == TEXT("transform") || TrackType == TEXT("3dtransform"))
	{
		// Find the transform track by actor label
		FString ActorLabel;
		Params->TryGetStringField(TEXT("actor_label"), ActorLabel);

		// Parse transform values
		FVector Location(0, 0, 0);
		FRotator Rotation(0, 0, 0);
		FVector Scale(1, 1, 1);

		const TSharedPtr<FJsonObject>* LocObj;
		if (Params->TryGetObjectField(TEXT("location"), LocObj))
		{
			(*LocObj)->TryGetNumberField(TEXT("x"), Location.X);
			(*LocObj)->TryGetNumberField(TEXT("y"), Location.Y);
			(*LocObj)->TryGetNumberField(TEXT("z"), Location.Z);
		}

		const TSharedPtr<FJsonObject>* RotObj;
		if (Params->TryGetObjectField(TEXT("rotation"), RotObj))
		{
			(*RotObj)->TryGetNumberField(TEXT("pitch"), Rotation.Pitch);
			(*RotObj)->TryGetNumberField(TEXT("yaw"), Rotation.Yaw);
			(*RotObj)->TryGetNumberField(TEXT("roll"), Rotation.Roll);
		}

		const TSharedPtr<FJsonObject>* ScaleObj;
		if (Params->TryGetObjectField(TEXT("scale"), ScaleObj))
		{
			(*ScaleObj)->TryGetNumberField(TEXT("x"), Scale.X);
			(*ScaleObj)->TryGetNumberField(TEXT("y"), Scale.Y);
			(*ScaleObj)->TryGetNumberField(TEXT("z"), Scale.Z);
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

				if (BindingName != ActorLabel)
				{
					continue;
				}
			}

			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(Track);
				if (!TransformTrack) continue;

				for (UMovieSceneSection* Section : TransformTrack->GetAllSections())
				{
					UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Section);
					if (!TransformSection) continue;

					// Get the channel proxy to access individual channels
					TArrayView<FMovieSceneFloatChannel*> FloatChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

					// Channels: 0=TX, 1=TY, 2=TZ, 3=RX, 4=RY, 5=RZ, 6=SX, 7=SY, 8=SZ
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
						Report = FString::Printf(TEXT("Added transform keyframe at %.2fs: Loc(%s) Rot(%s) Scale(%s)"),
							TimeSeconds, *Location.ToString(), *Rotation.ToString(), *Scale.ToString());
					}
				}
			}
		}
	}

	if (KeysAdded == 0 && Report.IsEmpty())
	{
		Result.Errors.Add(TEXT("No keyframes were added. Check that the track exists and the actor_label matches."));
		return Result;
	}

	Sequence->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = Report;
	return Result;
}

#include "MoviePipelineQueue.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineOutputSetting.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"

FAgentFrameworkActionResult FAgentFrameworkSequencerActions::ExecuteConfigureMovieRenderJob(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString QueuePath = Params->GetStringField(TEXT("queue_path"));
	FString MapPath = Params->GetStringField(TEXT("map_path"));
	FString SequencePath = Params->GetStringField(TEXT("sequence_path"));
	FString OutputDir = Params->GetStringField(TEXT("output_dir"));

	UMoviePipelineQueue* Queue = LoadObject<UMoviePipelineQueue>(nullptr, *QueuePath);
	if (!Queue)
	{
		FString PackagePath = FPackageName::GetLongPackagePath(QueuePath);
		FString AssetName = FPackageName::GetShortName(QueuePath);
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		Queue = Cast<UMoviePipelineQueue>(AssetTools.CreateAsset(AssetName, PackagePath, UMoviePipelineQueue::StaticClass(), nullptr));
	}

	if (!Queue)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load or create MoviePipelineQueue at '%s'."), *QueuePath));
		return Result;
	}

	Queue->Modify();
	UMoviePipelineExecutorJob* NewJob = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
	if (NewJob)
	{
		NewJob->Map = FSoftObjectPath(MapPath);
		NewJob->Sequence = FSoftObjectPath(SequencePath);
		NewJob->JobName = TEXT("AgentFrameworkRenderJob");

		UMoviePipelinePrimaryConfig* MasterConfig = NewJob->GetConfiguration();
		if (MasterConfig)
		{
			UMoviePipelineOutputSetting* OutputSetting = Cast<UMoviePipelineOutputSetting>(
				MasterConfig->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
			if (OutputSetting)
			{
				OutputSetting->OutputDirectory.Path = OutputDir;
				OutputSetting->FileNameFormat = TEXT("{sequence_name}_{frame_number}");
			}
		}
	}

	UPackage* Package = Queue->GetOutermost();
	Package->MarkPackageDirty();
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::SavePackage(Package, Queue, *PackageFilename, SaveArgs);
	}
	FAssetRegistryModule::AssetCreated(Queue);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Configured Movie Render Job in queue '%s'."), *QueuePath);
	Result.ModifiedAssets.Add(QueuePath);
	return Result;
}

#undef LOCTEXT_NAMESPACE
