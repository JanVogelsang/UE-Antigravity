// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityFileContextTracker.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"

FAntigravityFileContextTracker::FAntigravityFileContextTracker()
{
}

FAntigravityFileContextTracker::~FAntigravityFileContextTracker()
{
	// Unregister all directory watchers
	if (bWatchersRegistered && FModuleManager::Get().IsModuleLoaded(TEXT("DirectoryWatcher")))
	{
		FDirectoryWatcherModule& WatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		IDirectoryWatcher* Watcher = WatcherModule.Get();
		if (Watcher)
		{
			for (auto& Pair : DirectoryWatcherHandles)
			{
				Watcher->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value);
			}
		}
	}
	DirectoryWatcherHandles.Empty();
}

void FAntigravityFileContextTracker::Initialize(const FString& InProjectDirectory)
{
	ProjectDirectory = FPaths::ConvertRelativePathToFull(InProjectDirectory);
	TrackedFiles.Empty();
	RecentlyEditedByAntigravity.Empty();
	DirectoryWatcherHandles.Empty();
	bWatchersRegistered = false;
}

void FAntigravityFileContextTracker::OnFileReadByAI(const FString& RelativePath)
{
	const FString Normalized = FPaths::GetCleanFilename(RelativePath).IsEmpty()
		? RelativePath
		: RelativePath.Replace(TEXT("\\"), TEXT("/"));

	FAntigravityTrackedFile& Entry = TrackedFiles.FindOrAdd(Normalized, FAntigravityTrackedFile(Normalized));
	Entry.LastReadByAI = FDateTime::UtcNow();
	Entry.bIsStale = false;  // Fresh read â€” mark as current

	// Set up file system watcher for this file's directory
	RegisterFileWatcher(Normalized);
}

void FAntigravityFileContextTracker::OnFileEditedByAntigravity(const FString& RelativePath)
{
	const FString Normalized = RelativePath.Replace(TEXT("\\"), TEXT("/"));

	// Mark as edited by us â€” the watcher will see this change but shouldn't flag it stale
	RecentlyEditedByAntigravity.Add(Normalized);

	FAntigravityTrackedFile* Entry = TrackedFiles.Find(Normalized);
	if (Entry)
	{
		Entry->LastEditedByAntigravity = FDateTime::UtcNow();
		Entry->bIsStale = false;  // We just wrote it â€” it's fresh
	}

	// Register watcher to track any subsequent user changes
	RegisterFileWatcher(Normalized);
}

TArray<FString> FAntigravityFileContextTracker::GetAndClearStaleFiles()
{
	TArray<FString> StaleFiles;
	for (auto& Pair : TrackedFiles)
	{
		if (Pair.Value.bIsStale)
		{
			StaleFiles.Add(Pair.Key);
			Pair.Value.bIsStale = false;
		}
	}
	return StaleFiles;
}

FString FAntigravityFileContextTracker::BuildStaleContextWarning()
{
	TArray<FString> StaleFiles = GetAndClearStaleFiles();
	if (StaleFiles.Num() == 0)
	{
		return FString();
	}

	FString Warning = TEXT("âš ï¸ STALE CONTEXT WARNING:\n");
	Warning += TEXT("The following files have been modified externally since you last read them.\n");
	Warning += TEXT("You MUST re-read them before making any edits to avoid applying changes to outdated content:\n");

	const FDateTime Now = FDateTime::UtcNow();
	for (const FString& FilePath : StaleFiles)
	{
		const FAntigravityTrackedFile* Entry = TrackedFiles.Find(FilePath);
		FString TimeAgo;
		if (Entry && Entry->LastModifiedByUser != FDateTime::MinValue())
		{
			const FTimespan Delta = Now - Entry->LastModifiedByUser;
			if (Delta.GetTotalSeconds() < 60.0)
			{
				TimeAgo = FString::Printf(TEXT(" (modified %.0f seconds ago)"), Delta.GetTotalSeconds());
			}
			else if (Delta.GetTotalMinutes() < 60.0)
			{
				TimeAgo = FString::Printf(TEXT(" (modified %.0f minutes ago)"), Delta.GetTotalMinutes());
			}
			else
			{
				TimeAgo = FString::Printf(TEXT(" (modified %.1f hours ago)"), Delta.GetTotalHours());
			}
		}
		Warning += FString::Printf(TEXT("  - %s%s\n"), *FilePath, *TimeAgo);
	}

	return Warning;
}

bool FAntigravityFileContextTracker::HasStaleFiles() const
{
	for (const auto& Pair : TrackedFiles)
	{
		if (Pair.Value.bIsStale)
		{
			return true;
		}
	}
	return false;
}

TArray<FString> FAntigravityFileContextTracker::GetTrackedPaths() const
{
	TArray<FString> Paths;
	Paths.Reserve(TrackedFiles.Num());
	for (const auto& Pair : TrackedFiles)
	{
		Paths.Add(Pair.Key);
	}
	return Paths;
}

void FAntigravityFileContextTracker::Reset()
{
	// Unregister watchers
	if (FModuleManager::Get().IsModuleLoaded(TEXT("DirectoryWatcher")))
	{
		FDirectoryWatcherModule& WatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		IDirectoryWatcher* Watcher = WatcherModule.Get();
		if (Watcher)
		{
			for (auto& Pair : DirectoryWatcherHandles)
			{
				Watcher->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value);
			}
		}
	}

	TrackedFiles.Empty();
	RecentlyEditedByAntigravity.Empty();
	DirectoryWatcherHandles.Empty();
	bWatchersRegistered = false;
}

void FAntigravityFileContextTracker::OnFileChangedExternally(const FString& AbsolutePath)
{
	const FString RelPath = ToRelative(AbsolutePath);

	// If this was a change we made ourselves, ignore it and clear the flag
	if (RecentlyEditedByAntigravity.Contains(RelPath))
	{
		RecentlyEditedByAntigravity.Remove(RelPath);
		return;
	}

	// Mark as stale if the AI has previously read this file
	FAntigravityTrackedFile* Entry = TrackedFiles.Find(RelPath);
	if (Entry)
	{
		Entry->LastModifiedByUser = FDateTime::UtcNow();
		Entry->bIsStale = true;
		UE_LOG(LogTemp, Verbose, TEXT("AntigravityFileContextTracker: File became stale: %s"), *RelPath);
	}
}

void FAntigravityFileContextTracker::RegisterFileWatcher(const FString& RelativePath)
{
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("DirectoryWatcher")))
	{
		return;
	}

	// Watch the parent directory (UE's watcher works at directory level)
	const FString AbsPath = ToAbsolute(RelativePath);
	const FString ParentDir = FPaths::GetPath(AbsPath);

	if (ParentDir.IsEmpty() || DirectoryWatcherHandles.Contains(ParentDir))
	{
		return;  // Already watching this directory
	}

	FDirectoryWatcherModule& WatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* DirWatcher = WatcherModule.Get();
	if (!DirWatcher)
	{
		return;
	}

	auto Callback = IDirectoryWatcher::FDirectoryChanged::CreateLambda(
		[this](const TArray<FFileChangeData>& Changes)
		{
			for (const FFileChangeData& Change : Changes)
			{
				if (Change.Action == FFileChangeData::FCA_Modified)
				{
					OnFileChangedExternally(Change.Filename);
				}
			}
		}
	);

	FDelegateHandle Handle;
	DirWatcher->RegisterDirectoryChangedCallback_Handle(ParentDir, Callback, Handle, /*Flags=*/0);
	DirectoryWatcherHandles.Add(ParentDir, Handle);
	bWatchersRegistered = true;
}

FString FAntigravityFileContextTracker::ToAbsolute(const FString& RelativePath) const
{
	return FPaths::Combine(ProjectDirectory, RelativePath);
}

FString FAntigravityFileContextTracker::ToRelative(const FString& AbsolutePath) const
{
	FString Relative = AbsolutePath;
	if (Relative.StartsWith(ProjectDirectory))
	{
		Relative = Relative.Mid(ProjectDirectory.Len());
		if (Relative.StartsWith(TEXT("/")) || Relative.StartsWith(TEXT("\\")))
		{
			Relative = Relative.Mid(1);
		}
	}
	Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
	return Relative;
}
