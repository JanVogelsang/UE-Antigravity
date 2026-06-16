// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityCoreModule.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogAntigravity);

#define LOCTEXT_NAMESPACE "FAntigravityCoreModule"

void FAntigravityCoreModule::StartupModule()
{
	// Prevent the editor from throttling when not the foreground window.
	// Agents interact with UE while it is in the background, so throttling
	// causes tools to time out or run extremely slowly.
	if (IConsoleVariable* IdleCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.IdleWhenNotForeground")))
	{
		SavedIdleWhenNotForeground = IdleCVar->GetInt();
		IdleCVar->Set(0, ECVF_SetByCode);
	}
	if (IConsoleVariable* FPSCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS.Unfocused")))
	{
		SavedMaxFPSUnfocused = FPSCVar->GetFloat();
		FPSCVar->Set(30.0f, ECVF_SetByCode);
	}

#if WITH_EDITOR
	// Disable background CPU throttling in Editor to prevent automation tests from hanging.
	// Uses reflection to avoid a hard dependency on UnrealEd.
	if (UClass* EditorPerfClass = LoadObject<UClass>(nullptr, TEXT("/Script/UnrealEd.EditorPerformanceSettings")))
	{
		if (UObject* EditorSettings = EditorPerfClass->GetDefaultObject())
		{
			if (FBoolProperty* UseLessCPUProp = FindFProperty<FBoolProperty>(EditorPerfClass, TEXT("bThrottleCPUWhenNotForeground")))
			{
				UseLessCPUProp->SetPropertyValue_InContainer(EditorSettings, false);
				EditorSettings->SaveConfig();
			}
		}
	}
#endif

	UE_LOG(LogAntigravity, Log, TEXT("AntigravityCore module started. Unattended mode enabled, background throttling disabled."));
}

void FAntigravityCoreModule::ShutdownModule()
{
	if (IConsoleVariable* IdleCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.IdleWhenNotForeground")))
	{
		IdleCVar->Set(SavedIdleWhenNotForeground, ECVF_SetByCode);
	}
	if (IConsoleVariable* FPSCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS.Unfocused")))
	{
		FPSCVar->Set(SavedMaxFPSUnfocused, ECVF_SetByCode);
	}

	UE_LOG(LogAntigravity, Log, TEXT("AntigravityCore module shut down. All settings restored."));
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
