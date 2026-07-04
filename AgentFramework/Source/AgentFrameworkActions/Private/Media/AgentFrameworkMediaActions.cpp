// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Media/AgentFrameworkMediaActions.h"
#include "AgentFrameworkCoreModule.h"

FAgentFrameworkMediaActions::FAgentFrameworkMediaActions() {}
FAgentFrameworkMediaActions::~FAgentFrameworkMediaActions() {}
FName FAgentFrameworkMediaActions::GetActionName() const { return FName(TEXT("Media")); }
FAgentFrameworkActionResult FAgentFrameworkMediaActions::ExecuteAction(const TSharedRef<FJsonObject>& Params) { FAgentFrameworkActionResult R; R.ResultMessage = TEXT("Stub: not yet implemented"); return R; }
TArray<FString> FAgentFrameworkMediaActions::GetSupportedToolNames() const { return TArray<FString>(); }
bool FAgentFrameworkMediaActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const { return true; }
