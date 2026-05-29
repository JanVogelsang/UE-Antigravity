// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityActionsModule.h"
#include "AntigravityCoreModule.h"
#include "AntigravityHttpServer.h"

#define LOCTEXT_NAMESPACE "FAntigravityActionsModule"

void FAntigravityActionsModule::StartupModule() { 
	UE_LOG(LogAntigravity, Log, TEXT("AntigravityActions module started.")); 
	FAntigravityHttpServer::Start();
}

void FAntigravityActionsModule::ShutdownModule() { 
	FAntigravityHttpServer::Stop();
	UE_LOG(LogAntigravity, Log, TEXT("AntigravityActions module shut down.")); 
}
FAntigravityActionsModule& FAntigravityActionsModule::Get() { return FModuleManager::LoadModuleChecked<FAntigravityActionsModule>("AntigravityActions"); }
bool FAntigravityActionsModule::IsAvailable() { return FModuleManager::Get().IsModuleLoaded("AntigravityActions"); }

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAntigravityActionsModule, AntigravityActions)
