// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityTypes.h"

/**
 * Captures real-time editor context: selected actors, active level,
 * open editor windows, viewport camera. Injected into the system prompt
 * so Claude knows what the user is looking at.
 */
class ANTIGRAVITYENGINE_API FAntigravityEditorContextCapture
{
public:
	FAntigravityEditorContextCapture();
	~FAntigravityEditorContextCapture();

	/** Build a snapshot of the current editor state */
	FAntigravityEditorContext CaptureContext();

	/** Build a context string suitable for the system prompt */
	FString BuildContextString();
};
