// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

/**
 * FAgentFrameworkBehaviorTreeActions
 *
 * Gameplay AI tools for creating and configuring Behavior Trees:
 *   - create_blackboard: Create Blackboard assets with typed keys
 *   - create_behavior_tree: Create Behavior Tree assets and assign Blackboards
 *   - inject_bt_nodes: Add Selectors, Sequences, Decorators, Services programmatically
 *   - configure_navmesh: Spawn NavMeshBoundsVolumes and trigger NavMesh builds
 *
 * Behavior Trees are Unreal's primary AI decision-making system. Combined with
 * the Blackboard (shared memory), they let the AI create NPC behavior patterns
 * like patrol â†’ investigate â†’ attack â†’ flee.
 */
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkBehaviorTreeActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkBehaviorTreeActions();
	virtual ~FAgentFrameworkBehaviorTreeActions();

	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/** Create a Blackboard asset with typed keys (Bool, Int, Float, String, Vector, Object, Enum, Class). */
	FAgentFrameworkActionResult ExecuteCreateBlackboard(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Create a Behavior Tree asset and optionally assign a Blackboard. */
	FAgentFrameworkActionResult ExecuteCreateBehaviorTree(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Inject nodes into an existing BT (Selectors, Sequences, Tasks, Decorators, Services). */
	FAgentFrameworkActionResult ExecuteInjectBTNodes(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

	/** Spawn a NavMeshBoundsVolume, scale to level, and trigger NavMesh rebuild. */
	FAgentFrameworkActionResult ExecuteConfigureNavMesh(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
};
