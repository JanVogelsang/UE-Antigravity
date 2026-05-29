// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

/**
 * FAntigravityViewportActions
 *
 * Provides the capture_viewport tool - gives the AI "eyes" by capturing
 * the active editor viewport as a screenshot.
 *
 * The captured image is saved to disk and its path is returned, allowing Antigravity
 * to natively ingest the image using its multimodal capabilities without
 * large base64 proxy overhead.
 *
 * The AI can:
 *   - Visually inspect UMG widget layouts it just built
 *   - Check lighting and material appearance in-viewport
 *   - Verify actor placement and level design
 *   - Identify misaligned UI elements and fix them autonomously
 *
 * IMPLEMENTATION:
 *   Uses FViewport::ReadPixels() on the active level editor viewport to capture
 *   the current frame, then saves it as a JPEG to the project's scratch directory.
 */
class ANTIGRAVITYACTIONS_API FAntigravityViewportActions : public IAntigravityActionExecutor
{
public:
	FAntigravityViewportActions();
	virtual ~FAntigravityViewportActions();

	// IAntigravityActionExecutor interface
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
	 * Capture the active editor viewport and return a Base64-encoded JPEG.
	 *
	 * @param Params   JSON with optional:
	 *                   "max_dimension" (int, default 512, range 256-1024)
	 *                   "quality" (int, JPEG quality 30-95, default 75)
	 *                   "viewport_index" (int, default 0)
	 * @param Result   Action result to populate
	 * @return         Populated result with Base64 JPEG in ResultMessage
	 */
	FAntigravityActionResult ExecuteCaptureViewport(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result);

public:
	/**
	 * Save raw pixel data to a JPEG file on disk.
	 *
	 * @param Pixels        Raw BGRA8 pixel data
	 * @param Width         Image width
	 * @param Height        Image height
	 * @param MaxDimension  Maximum width/height for downscaling (default 512)
	 * @param JpegQuality   JPEG compression quality 0-100 (default 75)
	 * @return              Absolute path to the saved JPEG file, empty on failure
	 */
	static FString SavePixelsToDisk(
		const TArray<FColor>& Pixels,
		int32 Width,
		int32 Height,
		int32 MaxDimension,
		int32 JpegQuality = 75);

	/**
	 * Encode raw pixel data to a Base64 JPEG string.
	 *
	 * @param Pixels        Raw BGRA8 pixel data
	 * @param Width         Image width
	 * @param Height        Image height
	 * @param MaxDimension  Maximum width/height for downscaling (default 512)
	 * @param JpegQuality   JPEG compression quality 0-100 (default 75)
	 * @return              Base64 encoded string, empty on failure
	 */
	static FString EncodePixelsToBase64(
		const TArray<FColor>& Pixels,
		int32 Width,
		int32 Height,
		int32 MaxDimension,
		int32 JpegQuality = 75);
};
