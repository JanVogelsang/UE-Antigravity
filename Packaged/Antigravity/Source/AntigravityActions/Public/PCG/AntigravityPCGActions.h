// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * FAntigravityPCGActions
 *
 * Handles Procedural Content Generation (PCG) tool calls from the AI.
 * Requires UE 5.2+ and the PCG plugin to be enabled in the project.
 *
 * Capabilities (Tier A â€” graph assignment, parameters, generation):
 *   - create_pcg_graph: Create a new empty PCG graph asset
 *   - attach_pcg_component: Add a UPCGComponent to an actor in the level and assign a graph
 *   - set_pcg_parameter: Set an exposed parameter on a PCG component
 *   - generate_pcg_local: Call GenerateLocal(bForce=true) on a PCG component
 *   - get_pcg_info: Read-only query â€” return PCG component state, parameters, output actors
 *
 * Architecture notes:
 *   - PCG graph authoring (adding/connecting nodes inside the graph) requires the editor graph
 *     UI; Antigravity covers the workflow of assigning existing graphs and triggering generation.
 *   - GenerateLocal is non-blocking internally (async tasks) but the tool waits briefly and
 *     returns the current output actor count for immediate feedback.
 *   - The PCG plugin is optional. All executor methods guard against missing module.
 *   - PCG is UE5-only. This executor will return an error on UE4 builds.
 */
class ANTIGRAVITYACTIONS_API FAntigravityPCGActions : public IAntigravityActionExecutor
{
public:
	FAntigravityPCGActions();
	virtual ~FAntigravityPCGActions();

	// IAntigravityActionExecutor
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
	/**
	 * Create a new empty PCG graph asset at the given content path.
	 *
	 * Parameters:
	 *   - asset_path (string, required): Output content path, e.g. /Game/PCG/PCG_Forest
	 */
	FAntigravityActionResult ExecuteCreatePCGGraph(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/**
	 * Attach a UPCGComponent to an actor in the current level and optionally assign a PCG graph.
	 * The actor must already exist in the level.
	 *
	 * Parameters:
	 *   - actor_name (string, required): Name of the actor in the current level
	 *   - graph_path (string, optional): Content path of the PCG graph asset to assign
	 */
	FAntigravityActionResult ExecuteAttachPCGComponent(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/**
	 * Set an exposed parameter on a PCG component's graph instance.
	 * The parameter must be exposed in the PCG graph asset.
	 *
	 * Parameters:
	 *   - actor_name (string, required): Name of the actor owning the PCG component
	 *   - parameter_name (string, required): Exact name of the exposed parameter
	 *   - parameter_value (string, required): String-serialized value to set
	 *   - parameter_type (string, optional): "float", "int", "bool", "string", "vector" â€” helps resolve the type
	 */
	FAntigravityActionResult ExecuteSetPCGParameter(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/**
	 * Trigger local PCG generation on an actor's PCG component.
	 * Calls UPCGComponent::GenerateLocal(bForce=true).
	 *
	 * Parameters:
	 *   - actor_name (string, required): Name of the actor with the PCG component
	 *   - force (bool, optional): Force regeneration even if clean. Default: true.
	 */
	FAntigravityActionResult ExecuteGeneratePCGLocal(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/**
	 * Read-only query â€” return the PCG component state of an actor.
	 * Returns: graph path, exposed parameters list, last generation status, output actor count.
	 *
	 * Parameters:
	 *   - actor_name (string, required): Name of the actor in the current level
	 */
	FAntigravityActionResult ExecuteGetPCGInfo(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

	/** Find an actor in the current editor world by name. */
	static AActor* FindActorByName(const FString& ActorName);

	/** Check if the PCG plugin is available. Returns false with an error if not. */
	static bool CheckPCGAvailable(FAntigravityActionResult& Result);
};
