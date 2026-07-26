// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentFrameworkInterfaces.h"

class SLevelViewport;
class FLevelEditorViewportClient;

/**
 * FAgentFrameworkViewportActions
 *
 * Provides viewport control tools for AgentFramework AI assistant:
 *   - capture_viewport: Takes a screenshot of the active level viewport for vision ingestion.
 *   - set_viewport_camera: Adjusts camera position, rotation, and speed setting.
 *   - set_viewport_view_mode: Sets view mode (lit, unlit, wireframe, detail_lighting, etc.).
 *   - set_viewport_realtime: Toggles realtime viewport rendering.
 *   - focus_viewport_on_selection: Focuses camera onto currently selected actors.
 */
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkViewportActions : public IAgentFrameworkActionExecutor
{
public:
	FAgentFrameworkViewportActions();
	virtual ~FAgentFrameworkViewportActions();

	// IAgentFrameworkActionExecutor interface
	virtual FName GetActionName() const override;
	virtual FAgentFrameworkActionResult ExecuteAction(const TSharedRef<FJsonObject>& Params) override;
	virtual TArray<FString> GetSupportedToolNames() const override;
	virtual bool ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const override;

private:
	/** Helper to retrieve the active SLevelViewport with error reporting */
	TSharedPtr<SLevelViewport> GetActiveLevelViewport(TArray<FString>& OutErrors) const;

	FAgentFrameworkActionResult ExecuteCaptureViewport(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteSetViewportCamera(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteSetViewportViewMode(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteSetViewportRealtime(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
	FAgentFrameworkActionResult ExecuteFocusViewportOnSelection(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);

public:
	/**
	 * Save raw pixel data to a JPEG file on disk.
	 */
	static FString SavePixelsToDisk(
		const TArray<FColor>& Pixels,
		int32 Width,
		int32 Height,
		int32 MaxDimension,
		int32 JpegQuality = 75);
};
