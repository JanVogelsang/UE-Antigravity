// Copyright 2026 AgentFramework. All Rights Reserved.

#include "AgentFrameworkEngineModule.h"
#include "AgentFrameworkCoreModule.h"

#define LOCTEXT_NAMESPACE "FAgentFrameworkEngineModule"

void FAgentFrameworkEngineModule::StartupModule() { UE_LOG(LogAgentFramework, Log, TEXT("AgentFrameworkEngine module started.")); }
void FAgentFrameworkEngineModule::ShutdownModule() { UE_LOG(LogAgentFramework, Log, TEXT("AgentFrameworkEngine module shut down.")); }
FAgentFrameworkEngineModule& FAgentFrameworkEngineModule::Get() { return FModuleManager::LoadModuleChecked<FAgentFrameworkEngineModule>("AgentFrameworkEngine"); }
bool FAgentFrameworkEngineModule::IsAvailable() { return FModuleManager::Get().IsModuleLoaded("AgentFrameworkEngine"); }

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAgentFrameworkEngineModule, AgentFrameworkEngine)
