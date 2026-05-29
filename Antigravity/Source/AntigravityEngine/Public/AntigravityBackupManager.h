// Copyright 2026 Antigravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AntigravityInterfaces.h"

class ANTIGRAVITYENGINE_API FAntigravityBackupManager : public IAntigravityBackupManager
{
public:
	FAntigravityBackupManager();
	virtual ~FAntigravityBackupManager();

	virtual FString BackupFile(const FString& FilePath) override;
	virtual TArray<FString> BackupFiles(const TArray<FString>& FilePaths) override;
	virtual bool RestoreFile(const FString& BackupPath) override;
	virtual bool RestoreUndoGroup(const FString& UndoGroupName) override;
	virtual FString GetBackupDirectory() const override;
	virtual void PruneOldBackups() override;

	int32 MaxBackupCount = 50;

private:
	TMap<FString, TArray<FString>> UndoGroupBackups;
};
