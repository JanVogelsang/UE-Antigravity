// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

/**
 * FAgentFrameworkSequencerActions
 *
 * Provides tools for creating and editing Level Sequences (cinematics/timelines):
 *   - create_level_sequence: Create a LevelSequence asset and optionally spawn it
 *   - add_sequencer_track: Add Actor, Camera, Transform, or Audio tracks
 *   - add_sequencer_keyframe: Animate properties over time (transforms, floats, etc.)
 *
 * Level Sequences are used for cutscenes, camera animations, gameplay timelines,
 * and any property animation that needs precise timing control.
 */
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkSequencerActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkSequencerActions();
	virtual ~FAgentFrameworkSequencerActions();

	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/** Create a new LevelSequence asset. */
	FAgentFrameworkActionResult ExecuteCreateLevelSequence(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Add a track to an existing LevelSequence (Actor binding, Camera cut, Audio, etc.). */
	FAgentFrameworkActionResult ExecuteAddSequencerTrack(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Add a keyframe to a track property at a specific time. */
	FAgentFrameworkActionResult ExecuteAddSequencerKeyframe(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	FAgentFrameworkActionResult ExecuteConfigureMovieRenderJob(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
};
