// Copyright 2026 Antigravity. All Rights Reserved.

#include "Viewport/AntigravityViewportActions.h"
#include "AntigravityCoreModule.h"
#include "AntigravitySettings.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Misc/Base64.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

#define LOCTEXT_NAMESPACE "AntigravityViewportActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAntigravityViewportActions::FAntigravityViewportActions() {}
FAntigravityViewportActions::~FAntigravityViewportActions() {}

// ============================================================================
// IAntigravityActionExecutor Interface
// ============================================================================

FName FAntigravityViewportActions::GetActionName() const { return FName(TEXT("Viewport")); }

TArray<FString> FAntigravityViewportActions::GetSupportedToolNames() const
{
	return { TEXT("capture_viewport") };
}

bool FAntigravityViewportActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	// No required params â€” all optional
	return true;
}

FAntigravityActionResult FAntigravityViewportActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionResult Result;
	Result.bSuccess = false;
	return ExecuteCaptureViewport(Params, Result);
}

// ============================================================================
// capture_viewport
// ============================================================================

FAntigravityActionResult FAntigravityViewportActions::ExecuteCaptureViewport(
	const TSharedRef<FJsonObject>& Params,
	FAntigravityActionResult& Result)
{
	// Get optional parameters
	// Default to 512px to keep base64 output within context window budgets.
	// A 512px JPEG is ~30-80KB of base64 (~8K-20K tokens) â€” safe for 200K context.
	// A 1024px PNG was ~500KB-2MB of base64 (~125K-500K tokens) â€” would blow 200K context.
	int32 MaxDimension = 512;
	Params->TryGetNumberField(TEXT("max_dimension"), MaxDimension);
	MaxDimension = FMath::Clamp(MaxDimension, 256, 1024);

	int32 ViewportIndex = 0;
	Params->TryGetNumberField(TEXT("viewport_index"), ViewportIndex);

	// Get the level editor viewport
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SLevelViewport> LevelViewport;

	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if (!LevelEditor.IsValid())
	{
		Result.Errors.Add(TEXT("No Level Editor is active. Cannot capture viewport."));
		return Result;
	}

	// Get the active viewport (index 0 = primary viewport)
	TSharedPtr<SLevelViewport> ActiveViewport = LevelEditor->GetActiveViewportInterface();
	if (!ActiveViewport.IsValid())
	{
		Result.Errors.Add(TEXT("No active viewport found in the Level Editor."));
		return Result;
	}

	// Get the FViewport from the Slate viewport widget
	FViewport* Viewport = ActiveViewport->GetActiveViewport();
	if (!Viewport)
	{
		Result.Errors.Add(TEXT("Could not get FViewport from the active Level Viewport."));
		return Result;
	}

	// Read pixels from the viewport
	int32 Width = Viewport->GetSizeXY().X;
	int32 Height = Viewport->GetSizeXY().Y;

	if (Width <= 0 || Height <= 0)
	{
		Result.Errors.Add(TEXT("Viewport has zero dimensions. It may be minimized or not yet rendered."));
		return Result;
	}

	TArray<FColor> Pixels;
	if (!Viewport->ReadPixels(Pixels))
	{
		Result.Errors.Add(TEXT("Failed to read pixels from the viewport. The viewport may not have rendered yet."));
		return Result;
	}

	if (Pixels.Num() != Width * Height)
	{
		Result.Errors.Add(FString::Printf(TEXT("Pixel count mismatch: expected %d, got %d"), Width * Height, Pixels.Num()));
		return Result;
	}

	UE_LOG(LogAntigravity, Log, TEXT("ViewportActions: Captured viewport %dx%d (%d pixels)"), Width, Height, Pixels.Num());

	// Use JPEG quality 75 â€” much smaller than PNG for viewport screenshots.
	// A 512px JPEG at Q75 is typically 20-60KB â†’ ~27-80K base64 chars â†’ ~7-20K tokens.
	// A 512px PNG was 200-800KB â†’ ~270K-1M base64 chars â†’ ~67-250K tokens.
	// This prevents context window overflow on 200K models.
	int32 JpegQuality = 75;
	Params->TryGetNumberField(TEXT("quality"), JpegQuality);
	JpegQuality = FMath::Clamp(JpegQuality, 30, 95);

	FString FilePath = SavePixelsToDisk(Pixels, Width, Height, MaxDimension, JpegQuality);
	if (FilePath.IsEmpty())
	{
		Result.Errors.Add(TEXT("Failed to save viewport capture to JPEG."));
		return Result;
	}

	// Return the image path
	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(
		TEXT("[IMAGE:%s]\n\n"
			 "Viewport captured successfully (%dx%d, resized to max %dpx, JPEG quality %d). "
			 "The image shows the current editor viewport and has been saved to the path above."),
		*FilePath, Width, Height, MaxDimension, JpegQuality);

	return Result;
}

// ============================================================================
// Image Encoding (JPEG default â€” much smaller than PNG for viewport content)
// ============================================================================

FString FAntigravityViewportActions::SavePixelsToDisk(
	const TArray<FColor>& Pixels,
	int32 Width,
	int32 Height,
	int32 MaxDimension,
	int32 JpegQuality)
{
	if (Pixels.Num() == 0 || Width <= 0 || Height <= 0) return FString();

	// Determine if we need to resize
	int32 OutWidth = Width;
	int32 OutHeight = Height;
	int32 LongestEdge = FMath::Max(Width, Height);

	TArray<FColor> ResizedPixels;
	const TArray<FColor>* PixelsToEncode = &Pixels;

	if (LongestEdge > MaxDimension)
	{
		float Scale = static_cast<float>(MaxDimension) / static_cast<float>(LongestEdge);
		OutWidth = FMath::Max(1, FMath::RoundToInt(Width * Scale));
		OutHeight = FMath::Max(1, FMath::RoundToInt(Height * Scale));

		// Simple bilinear downscale
		ResizedPixels.SetNumUninitialized(OutWidth * OutHeight);
		for (int32 Y = 0; Y < OutHeight; ++Y)
		{
			for (int32 X = 0; X < OutWidth; ++X)
			{
				float SrcX = static_cast<float>(X) * Width / OutWidth;
				float SrcY = static_cast<float>(Y) * Height / OutHeight;
				int32 SX = FMath::Clamp(FMath::FloorToInt(SrcX), 0, Width - 1);
				int32 SY = FMath::Clamp(FMath::FloorToInt(SrcY), 0, Height - 1);
				ResizedPixels[Y * OutWidth + X] = Pixels[SY * Width + SX];
			}
		}
		PixelsToEncode = &ResizedPixels;
	}

	// Use JPEG encoding (much smaller than PNG for photographic/3D viewport content)
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogAntigravity, Error, TEXT("ViewportActions: Failed to create JPEG ImageWrapper"));
		return FString();
	}

	// Set raw BGRA data
	TArray<uint8> RawData;
	RawData.SetNumUninitialized(OutWidth * OutHeight * 4);
	for (int32 i = 0; i < PixelsToEncode->Num(); ++i)
	{
		const FColor& C = (*PixelsToEncode)[i];
		RawData[i * 4 + 0] = C.B;
		RawData[i * 4 + 1] = C.G;
		RawData[i * 4 + 2] = C.R;
		RawData[i * 4 + 3] = C.A;
	}

	if (!ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), OutWidth, OutHeight, ERGBFormat::BGRA, 8))
	{
		UE_LOG(LogAntigravity, Error, TEXT("ViewportActions: Failed to set raw pixel data on ImageWrapper"));
		return FString();
	}

	// Get compressed JPEG data
	TArray<uint8> CompressedData;
	CompressedData = ImageWrapper->GetCompressed(JpegQuality);

	if (CompressedData.Num() == 0)
	{
		UE_LOG(LogAntigravity, Error, TEXT("ViewportActions: JPEG compression returned empty data"));
		return FString();
	}

	UE_LOG(LogAntigravity, Log, TEXT("ViewportActions: JPEG encoded %dx%d Q%d -> %d bytes (vs PNG would be ~%dx larger)"),
		OutWidth, OutHeight, JpegQuality, CompressedData.Num(), 5);

	FString SaveDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT(".antigravity"), TEXT("scratch"));
	IFileManager::Get().MakeDirectory(*SaveDirectory, true);
	FString FilePath = FPaths::Combine(SaveDirectory, TEXT("viewport_capture.jpg"));
	
	if (FFileHelper::SaveArrayToFile(CompressedData, *FilePath))
	{
		return FPaths::ConvertRelativePathToFull(FilePath);
	}
	
	UE_LOG(LogAntigravity, Error, TEXT("ViewportActions: Failed to save JPEG to disk at %s"), *FilePath);
	return FString();
}

FString FAntigravityViewportActions::EncodePixelsToBase64(
	const TArray<FColor>& Pixels,
	int32 Width,
	int32 Height,
	int32 MaxDimension,
	int32 JpegQuality)
{
	if (Pixels.Num() == 0 || Width <= 0 || Height <= 0) return FString();

	// Determine if we need to resize
	int32 OutWidth = Width;
	int32 OutHeight = Height;
	int32 LongestEdge = FMath::Max(Width, Height);

	TArray<FColor> ResizedPixels;
	const TArray<FColor>* PixelsToEncode = &Pixels;

	if (LongestEdge > MaxDimension)
	{
		float Scale = static_cast<float>(MaxDimension) / static_cast<float>(LongestEdge);
		OutWidth = FMath::Max(1, FMath::RoundToInt(Width * Scale));
		OutHeight = FMath::Max(1, FMath::RoundToInt(Height * Scale));

		// Simple bilinear downscale
		ResizedPixels.SetNumUninitialized(OutWidth * OutHeight);
		for (int32 Y = 0; Y < OutHeight; ++Y)
		{
			for (int32 X = 0; X < OutWidth; ++X)
			{
				float SrcX = static_cast<float>(X) * Width / OutWidth;
				float SrcY = static_cast<float>(Y) * Height / OutHeight;
				int32 SX = FMath::Clamp(FMath::FloorToInt(SrcX), 0, Width - 1);
				int32 SY = FMath::Clamp(FMath::FloorToInt(SrcY), 0, Height - 1);
				ResizedPixels[Y * OutWidth + X] = Pixels[SY * Width + SX];
			}
		}
		PixelsToEncode = &ResizedPixels;
	}

	// Use JPEG encoding
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogAntigravity, Error, TEXT("ViewportActions: Failed to create JPEG ImageWrapper for Base64 encoding"));
		return FString();
	}

	// Set raw BGRA data
	TArray<uint8> RawData;
	RawData.SetNumUninitialized(OutWidth * OutHeight * 4);
	for (int32 i = 0; i < PixelsToEncode->Num(); ++i)
	{
		const FColor& C = (*PixelsToEncode)[i];
		RawData[i * 4 + 0] = C.B;
		RawData[i * 4 + 1] = C.G;
		RawData[i * 4 + 2] = C.R;
		RawData[i * 4 + 3] = C.A;
	}

	if (!ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), OutWidth, OutHeight, ERGBFormat::BGRA, 8))
	{
		UE_LOG(LogAntigravity, Error, TEXT("ViewportActions: Failed to set raw pixel data on ImageWrapper for Base64 encoding"));
		return FString();
	}

	// Get compressed JPEG data
	TArray<uint8> CompressedData;
	CompressedData = ImageWrapper->GetCompressed(JpegQuality);

	if (CompressedData.Num() == 0)
	{
		UE_LOG(LogAntigravity, Error, TEXT("ViewportActions: JPEG compression returned empty data for Base64 encoding"));
		return FString();
	}

	return FBase64::Encode(CompressedData);
}

#undef LOCTEXT_NAMESPACE
