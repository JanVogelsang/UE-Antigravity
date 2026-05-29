// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityEngineModule.h"
#include "AntigravityCoreModule.h"

#define LOCTEXT_NAMESPACE "FAntigravityEngineModule"

void FAntigravityEngineModule::StartupModule() { UE_LOG(LogAntigravity, Log, TEXT("AntigravityEngine module started.")); }
void FAntigravityEngineModule::ShutdownModule() { UE_LOG(LogAntigravity, Log, TEXT("AntigravityEngine module shut down.")); }
FAntigravityEngineModule& FAntigravityEngineModule::Get() { return FModuleManager::LoadModuleChecked<FAntigravityEngineModule>("AntigravityEngine"); }
bool FAntigravityEngineModule::IsAvailable() { return FModuleManager::Get().IsModuleLoaded("AntigravityEngine"); }

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAntigravityEngineModule, AntigravityEngine)
