// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityCoreModule.h"

DEFINE_LOG_CATEGORY(LogAntigravity);

#define LOCTEXT_NAMESPACE "FAntigravityCoreModule"

void FAntigravityCoreModule::StartupModule()
{
	UE_LOG(LogAntigravity, Log, TEXT("AntigravityCore module started."));
}

void FAntigravityCoreModule::ShutdownModule()
{
	UE_LOG(LogAntigravity, Log, TEXT("AntigravityCore module shut down."));
}

FAntigravityCoreModule& FAntigravityCoreModule::Get()
{
	return FModuleManager::LoadModuleChecked<FAntigravityCoreModule>("AntigravityCore");
}

bool FAntigravityCoreModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("AntigravityCore");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAntigravityCoreModule, AntigravityCore)
