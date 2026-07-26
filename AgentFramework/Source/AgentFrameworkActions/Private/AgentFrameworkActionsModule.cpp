// Copyright 2026 AgentFramework. All Rights Reserved.

#include "AgentFrameworkActionsModule.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkHttpServer.h"
#include "AgentFrameworkActionUtils.h"
#include "AgentFrameworkActionRouter.h"
#include "Misc/CoreDelegates.h"
#include "UObject/Package.h"

TSet<FName> FAgentFrameworkActionsModule::AgentDirtiedPackages;

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/AgentFrameworkBlueprintActions.h"
static FDelegateHandle ExtraTagsDelegateHandle;
#endif

#define LOCTEXT_NAMESPACE "FAgentFrameworkActionsModule"

void FAgentFrameworkActionsModule::StartupModule() { 
	UE_LOG(LogAgentFramework, Log, TEXT("AgentFrameworkActions module started.")); 

	// Bind ActionRouter telemetry delegate to ActionUtils telemetry recorder
	FAgentFrameworkActionRouter::OnToolExecutionRecorded.AddStatic(&UAgentFrameworkActionUtils::RecordToolExecution); 

#if WITH_EDITOR
	ExtraTagsDelegateHandle = UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.AddStatic(&FAgentFrameworkBlueprintActions::HandleGetExtraObjectTags);
	UPackage::PackageDirtyStateChangedEvent.AddRaw(this, &FAgentFrameworkActionsModule::OnPackageDirtyStateChanged);
#endif

	if (!IsRunningCommandlet())
	{
		if (GEngine)
		{
			FAgentFrameworkHttpServer::Start();
		}
		else
		{
			FCoreDelegates::OnPostEngineInit.AddLambda([]() {
				FAgentFrameworkHttpServer::Start();
			});
		}
	}
}

void FAgentFrameworkActionsModule::ShutdownModule() { 
#if WITH_EDITOR
	if (ExtraTagsDelegateHandle.IsValid())
	{
		UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.Remove(ExtraTagsDelegateHandle);
		ExtraTagsDelegateHandle.Reset();
	}
	UPackage::PackageDirtyStateChangedEvent.RemoveAll(this);
#endif
	FAgentFrameworkHttpServer::Stop();
	UE_LOG(LogAgentFramework, Log, TEXT("AgentFrameworkActions module shut down.")); 
}
FAgentFrameworkActionsModule& FAgentFrameworkActionsModule::Get() { return FModuleManager::LoadModuleChecked<FAgentFrameworkActionsModule>("AgentFrameworkActions"); }
bool FAgentFrameworkActionsModule::IsAvailable() { return FModuleManager::Get().IsModuleLoaded("AgentFrameworkActions"); }

#undef LOCTEXT_NAMESPACE

void FAgentFrameworkActionsModule::OnPackageDirtyStateChanged(UPackage* ModifiedPackage)
{
	if (ModifiedPackage && !ModifiedPackage->IsDirty())
	{
		AgentDirtiedPackages.Remove(ModifiedPackage->GetFName());
	}
}

IMPLEMENT_MODULE(FAgentFrameworkActionsModule, AgentFrameworkActions)
