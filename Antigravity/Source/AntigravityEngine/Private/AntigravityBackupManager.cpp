// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityBackupManager.h"
#include "AntigravityCoreModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

FAntigravityBackupManager::FAntigravityBackupManager() {}
FAntigravityBackupManager::~FAntigravityBackupManager() {}

FString FAntigravityBackupManager::GetBackupDirectory() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Antigravity"), TEXT("Backups"));
}

FString FAntigravityBackupManager::BackupFile(const FString& FilePath)
{
	FString BackupDir = FPaths::Combine(GetBackupDirectory(), FDateTime::UtcNow().ToString());
	FString FileName = FPaths::GetCleanFilename(FilePath);
	FString BackupPath = FPaths::Combine(BackupDir, FileName);
	IFileManager::Get().MakeDirectory(*BackupDir, true);
	IFileManager::Get().Copy(*BackupPath, *FilePath);
	UE_LOG(LogAntigravity, Log, TEXT("BackupManager: Backed up %s -> %s"), *FilePath, *BackupPath);
	return BackupPath;
}

TArray<FString> FAntigravityBackupManager::BackupFiles(const TArray<FString>& FilePaths)
{
	TArray<FString> BackupPaths;
	for (const FString& Path : FilePaths) { BackupPaths.Add(BackupFile(Path)); }
	return BackupPaths;
}

bool FAntigravityBackupManager::RestoreFile(const FString& BackupPath) { /* Stub */ return false; }
bool FAntigravityBackupManager::RestoreUndoGroup(const FString& UndoGroupName) { /* Stub */ return false; }
void FAntigravityBackupManager::PruneOldBackups() { /* Stub */ }
