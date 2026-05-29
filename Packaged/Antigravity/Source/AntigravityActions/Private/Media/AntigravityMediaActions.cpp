// Copyright 2026 Antigravity. All Rights Reserved.

#include "Media/AntigravityMediaActions.h"
#include "AntigravityCoreModule.h"

FAntigravityMediaActions::FAntigravityMediaActions() {}
FAntigravityMediaActions::~FAntigravityMediaActions() {}
FName FAntigravityMediaActions::GetActionName() const { return FName(TEXT("Media")); }
FText FAntigravityMediaActions::GetDisplayName() const { return FText::FromString(TEXT("Media Actions")); }
EAntigravityActionCategory FAntigravityMediaActions::GetCategory() const { return EAntigravityActionCategory::Texture; }
EAntigravityRiskLevel FAntigravityMediaActions::GetDefaultRiskLevel() const { return EAntigravityRiskLevel::Medium; }
FAntigravityActionPlan FAntigravityMediaActions::PreviewAction(const TSharedRef<FJsonObject>& Params) { return FAntigravityActionPlan(); }
FAntigravityActionResult FAntigravityMediaActions::ExecuteAction(const TSharedRef<FJsonObject>& Params) { FAntigravityActionResult R; R.ResultMessage = TEXT("Stub: not yet implemented"); return R; }
bool FAntigravityMediaActions::CanUndo() const { return false; }
bool FAntigravityMediaActions::UndoAction() { return false; }
TArray<FString> FAntigravityMediaActions::GetSupportedToolNames() const { return TArray<FString>(); }
bool FAntigravityMediaActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const { return true; }
