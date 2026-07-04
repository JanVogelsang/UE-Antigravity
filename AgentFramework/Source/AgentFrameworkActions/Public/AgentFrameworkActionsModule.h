// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UPackage;

class FAgentFrameworkActionsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	static FAgentFrameworkActionsModule& Get();
	static bool IsAvailable();

	static TSet<FName> AgentDirtiedPackages;
private:
	void OnPackageDirtyStateChanged(UPackage* ModifiedPackage);
};
