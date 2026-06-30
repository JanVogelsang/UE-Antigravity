// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UPackage;

class FAntigravityActionsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	static FAntigravityActionsModule& Get();
	static bool IsAvailable();

	static TSet<FName> AgentDirtiedPackages;
private:
	void OnPackageDirtyStateChanged(UPackage* ModifiedPackage);
};
