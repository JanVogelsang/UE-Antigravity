// Copyright 2026 Antigravity. All Rights Reserved.

#include "Media/AntigravityMediaActions.h"
#include "AntigravityCoreModule.h"

FAntigravityMediaActions::FAntigravityMediaActions() {}
FAntigravityMediaActions::~FAntigravityMediaActions() {}
FName FAntigravityMediaActions::GetActionName() const { return FName(TEXT("Media")); }
FAntigravityActionResult FAntigravityMediaActions::ExecuteAction(const TSharedRef<FJsonObject>& Params) { FAntigravityActionResult R; R.ResultMessage = TEXT("Stub: not yet implemented"); return R; }
TArray<FString> FAntigravityMediaActions::GetSupportedToolNames() const { return TArray<FString>(); }
bool FAntigravityMediaActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const { return true; }
