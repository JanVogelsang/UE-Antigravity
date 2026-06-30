// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityActionsModule.h"
#include "AntigravityCoreModule.h"
#include "AntigravityHttpServer.h"
#include "Misc/CoreDelegates.h"
#include "UObject/Package.h"

TSet<FName> FAntigravityActionsModule::AgentDirtiedPackages;

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/AntigravityBlueprintActions.h"
static FDelegateHandle ExtraTagsDelegateHandle;
#endif

#define LOCTEXT_NAMESPACE "FAntigravityActionsModule"

void FAntigravityActionsModule::StartupModule() { 
	UE_LOG(LogAntigravity, Log, TEXT("AntigravityActions module started.")); 

#if WITH_EDITOR
	ExtraTagsDelegateHandle = UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.AddStatic(&FAntigravityBlueprintActions::HandleGetExtraObjectTags);
	UPackage::PackageDirtyStateChangedEvent.AddRaw(this, &FAntigravityActionsModule::OnPackageDirtyStateChanged);
#endif

	if (!IsRunningCommandlet())
	{
		if (GEngine)
		{
			FAntigravityHttpServer::Start();
		}
		else
		{
			FCoreDelegates::OnPostEngineInit.AddLambda([]() {
				FAntigravityHttpServer::Start();
			});
		}
	}
}

void FAntigravityActionsModule::ShutdownModule() { 
#if WITH_EDITOR
	if (ExtraTagsDelegateHandle.IsValid())
	{
		UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.Remove(ExtraTagsDelegateHandle);
		ExtraTagsDelegateHandle.Reset();
	}
	UPackage::PackageDirtyStateChangedEvent.RemoveAll(this);
#endif
	FAntigravityHttpServer::Stop();
	UE_LOG(LogAntigravity, Log, TEXT("AntigravityActions module shut down.")); 
}
FAntigravityActionsModule& FAntigravityActionsModule::Get() { return FModuleManager::LoadModuleChecked<FAntigravityActionsModule>("AntigravityActions"); }
bool FAntigravityActionsModule::IsAvailable() { return FModuleManager::Get().IsModuleLoaded("AntigravityActions"); }

#undef LOCTEXT_NAMESPACE

void FAntigravityActionsModule::OnPackageDirtyStateChanged(UPackage* ModifiedPackage)
{
	if (ModifiedPackage && !ModifiedPackage->IsDirty())
	{
		AgentDirtiedPackages.Remove(ModifiedPackage->GetFName());
	}
}

IMPLEMENT_MODULE(FAntigravityActionsModule, AntigravityActions)
