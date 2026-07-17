// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

/**
 * FAgentFrameworkAnimationActions
 *
 * Handles animation asset tool calls from the AI.
 *
 * Capabilities:
 *   - create_anim_blueprint: Create an Animation Blueprint targeting a skeleton
 *   - import_animation_fbx: Import an FBX animation to a target skeleton
 *   - assign_anim_blueprint: Assign an AnimBP to a SkeletalMeshComponent in a Blueprint
 *   - create_anim_montage: Create an AnimMontage from one or more AnimSequences
 *   - get_anim_info: Read-only query â€” get skeleton, sequences, montages, ABP status
 *
 * Architecture notes:
 *   - UAnimBlueprint is created via UAnimBlueprintFactory (same factory pattern as Blueprint actors)
 *   - AnimBP has two isolated graphs: EventGraph (cached proxy variable reads) and AnimGraph
 *     (blend nodes, state machines, skeletal controls â€” NOT standard K2 nodes)
 *   - For AnimGraph authoring, prefer Blueprint â†’ Anim Graph workflow via the editor; AgentFramework
 *     populates the EventGraph (K2 logic) and sets asset references (AnimBP, Montage) on components
 *   - Import via UAssetImportTask (same as MeshActions); FBX file must exist on local disk
 */
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkAnimationActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkAnimationActions();
	virtual ~FAgentFrameworkAnimationActions();

	// IAgentFrameworkActionExecutor
	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/**
	 * Create an Animation Blueprint asset targeting a skeleton.
	 *
	 * Parameters:
	 *   - asset_path (string, required): Output path e.g. /Game/Animations/ABP_MyChar
	 *   - skeleton_path (string, required): Content path of the USkeleton asset
	 *   - parent_class (string, optional): Parent C++ class. Default: "AnimInstance"
	 */
	FAgentFrameworkActionResult ExecuteCreateAnimBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/**
	 * Import an FBX animation file to a target skeleton.
	 * Creates a UAnimSequence asset at destination_path.
	 *
	 * Parameters:
	 *   - source_file (string, required): Absolute path to the .fbx file on disk
	 *   - skeleton_path (string, required): Content path of the USkeleton to target
	 *   - destination_path (string, optional): Content folder for the created sequence. Default: /Game/Animations
	 *   - asset_name (string, optional): Name for the created AnimSequence asset
	 */
	FAgentFrameworkActionResult ExecuteImportAnimationFBX(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/**
	 * Assign an Animation Blueprint to a SkeletalMeshComponent inside a Blueprint.
	 *
	 * Parameters:
	 *   - asset_path (string, required): Content path of the Blueprint to modify
	 *   - component_name (string, required): Variable name of the SkeletalMeshComponent
	 *   - anim_blueprint_path (string, required): Content path of the _C generated class e.g. /Game/Animations/ABP_Char.ABP_Char_C
	 */
	FAgentFrameworkActionResult ExecuteAssignAnimBlueprint(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/**
	 * Create an AnimMontage from one or more AnimSequences on a skeleton.
	 *
	 * Parameters:
	 *   - asset_path (string, required): Output content path for the Montage
	 *   - skeleton_path (string, required): Content path of the USkeleton
	 *   - sequences (array of strings, required): Content paths of AnimSequences to include
	 */
	FAgentFrameworkActionResult ExecuteCreateAnimMontage(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/**
	 * Read-only query â€” return information about animation assets on a skeleton.
	 * Returns: skeleton name, compatible anim sequences list, montages list.
	 *
	 * Parameters:
	 *   - asset_path (string, required): Content path of a USkeleton or an AnimBlueprint
	 */
	FAgentFrameworkActionResult ExecuteGetAnimInfo(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	FAgentFrameworkActionResult ExecuteConfigureMotionMatching(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteCreateIKRig(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteCreateIKRetargeter(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteCreateControlRig(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteSetupMotionWarping(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteCreateBlendSpace(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteConfigureAnimMontage(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteMapLiveLinkSource(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
};
