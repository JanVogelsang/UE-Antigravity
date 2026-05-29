// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * FAntigravitySequencerActions
 *
 * Provides tools for creating and editing Level Sequences (cinematics/timelines):
 *   - create_level_sequence: Create a LevelSequence asset and optionally spawn it
 *   - add_sequencer_track: Add Actor, Camera, Transform, or Audio tracks
 *   - add_sequencer_keyframe: Animate properties over time (transforms, floats, etc.)
 *
 * Level Sequences are used for cutscenes, camera animations, gameplay timelines,
 * and any property animation that needs precise timing control.
 */
class ANTIGRAVITYACTIONS_API FAntigravitySequencerActions : public IAntigravityActionExecutor
{
public:
	FAntigravitySequencerActions();
	virtual ~FAntigravitySequencerActions();

	virtual FName GetActionName() const override;
	virtual FText GetDisplayName() const override;
	virtual EAntigravityActionCategory GetCategory() const override;
	virtual EAntigravityRiskLevel GetDefaultRiskLevel() const override;
	virtual FAntigravityActionPlan PreviewAction(const TSharedRef<FJsonObject>& Params) override;
	virtual FAntigravityActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual bool CanUndo() const override;
	virtual bool UndoAction() override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/** Create a new LevelSequence asset. */
	FAntigravityActionResult ExecuteCreateLevelSequence(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Add a track to an existing LevelSequence (Actor binding, Camera cut, Audio, etc.). */
	FAntigravityActionResult ExecuteAddSequencerTrack(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Add a keyframe to a track property at a specific time. */
	FAntigravityActionResult ExecuteAddSequencerKeyframe(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);
};
