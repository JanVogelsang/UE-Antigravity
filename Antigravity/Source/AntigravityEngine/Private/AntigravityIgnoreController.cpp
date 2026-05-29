// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityIgnoreController.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"

// ---------------------------------------------------------------------------
// Default .antigravityignore content shipped with the plugin
// ---------------------------------------------------------------------------
const TCHAR* FAntigravityIgnoreController::GetDefaultIgnoreContent()
{
	return
		TEXT("# Antigravity Ignore File\n")
		TEXT("# Uses .gitignore syntax. Files matching these patterns will be hidden from the AI.\n")
		TEXT("# Edit this file to customize AI access to your project files.\n")
		TEXT("\n")
		TEXT("# -----------------------------------------------------------------------\n")
		TEXT("# Binary Unreal assets -- never send to AI (binary blobs, not human-readable)\n")
		TEXT("# -----------------------------------------------------------------------\n")
		TEXT("*.uasset\n")
		TEXT("*.umap\n")
		TEXT("*.pak\n")
		TEXT("*.sig\n")
		TEXT("*.ucas\n")
		TEXT("*.utoc\n")
		TEXT("\n")
		TEXT("# -----------------------------------------------------------------------\n")
		TEXT("# Build artifacts\n")
		TEXT("# -----------------------------------------------------------------------\n")
		TEXT("Binaries/\n")
		TEXT("Intermediate/\n")
		TEXT("DerivedDataCache/\n")
		TEXT("Saved/\n")
		TEXT("\n")
		TEXT("# -----------------------------------------------------------------------\n")
		TEXT("# IDE and solution files\n")
		TEXT("# -----------------------------------------------------------------------\n")
		TEXT(".vs/\n")
		TEXT(".idea/\n")
		TEXT("*.sln\n")
		TEXT("*.suo\n")
		TEXT("*.vcxproj\n")
		TEXT("*.vcxproj.filters\n")
		TEXT("*.vcxproj.user\n")
		TEXT("\n")
		TEXT("# -----------------------------------------------------------------------\n")
		TEXT("# Package manager caches\n")
		TEXT("# -----------------------------------------------------------------------\n")
		TEXT("node_modules/\n")
		TEXT(".pnpm-store/\n")
		TEXT("\n")
		TEXT("# -----------------------------------------------------------------------\n")
		TEXT("# Large generated/cached data\n")
		TEXT("# -----------------------------------------------------------------------\n")
		TEXT("ShaderCache/\n")
		TEXT("*.bin\n")
		TEXT("*.pdb\n")
		TEXT("*.exp\n")
		TEXT("*.lib\n")
		TEXT("*.dll\n")
		TEXT("*.exe\n");
}

// ---------------------------------------------------------------------------

FAntigravityIgnoreController::FAntigravityIgnoreController()
{
}

FAntigravityIgnoreController::~FAntigravityIgnoreController()
{
	// Unregister file watcher
	if (bWatcherRegistered && FModuleManager::Get().IsModuleLoaded(TEXT("DirectoryWatcher")))
	{
		FDirectoryWatcherModule& WatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		IDirectoryWatcher* Watcher = WatcherModule.Get();
		if (Watcher)
		{
			Watcher->UnregisterDirectoryChangedCallback_Handle(GetIgnoreFilePath(), WatcherHandle);
		}
		bWatcherRegistered = false;
	}
}

bool FAntigravityIgnoreController::Initialize(const FString& InProjectDirectory)
{
	ProjectDirectory = FPaths::ConvertRelativePathToFull(InProjectDirectory);

	const FString IgnorePath = GetIgnoreFilePath();

	// Create default file if it doesn't exist
	if (!FPaths::FileExists(IgnorePath))
	{
		CreateDefaultIgnoreFile();
	}

	// Load patterns
	LoadIgnoreFile();

	// Register directory watcher to reload on .antigravityignore changes
	if (FModuleManager::Get().IsModuleLoaded(TEXT("DirectoryWatcher")))
	{
		FDirectoryWatcherModule& WatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		IDirectoryWatcher* DirWatcher = WatcherModule.Get();
		if (DirWatcher)
		{
			auto Callback = IDirectoryWatcher::FDirectoryChanged::CreateLambda([this](const TArray<FFileChangeData>& Changes)
			{
				for (const FFileChangeData& Change : Changes)
				{
					if (Change.Filename.EndsWith(TEXT(".antigravityignore")))
					{
						Reload();
						break;
					}
				}
			});

			DirWatcher->RegisterDirectoryChangedCallback_Handle(
				ProjectDirectory,
				Callback,
				WatcherHandle,
				/*Flags=*/0
			);
			bWatcherRegistered = true;
		}
	}

	return bIsLoaded;
}

void FAntigravityIgnoreController::Reload()
{
	LoadIgnoreFile();
}

FString FAntigravityIgnoreController::GetIgnoreFilePath() const
{
	return FPaths::Combine(ProjectDirectory, TEXT(".antigravityignore"));
}

bool FAntigravityIgnoreController::IsPathIgnored(const FString& RelativePath) const
{
	if (!bIsLoaded || IgnorePatterns.Num() == 0)
	{
		return false;
	}

	const FString Normalized = NormalizePath(RelativePath);
	if (Normalized.IsEmpty())
	{
		return false;
	}

	// Always ignore the .antigravityignore file itself
	if (Normalized == TEXT(".antigravityignore"))
	{
		return false; // The AI can SEE the ignore file, just not be blocked by it
	}

	bool bIgnored = false;
	for (const FString& Pattern : IgnorePatterns)
	{
		if (Pattern.StartsWith(TEXT("!")))
		{
			// Negation pattern â€” un-ignore if matches
			if (MatchesPattern(Normalized, Pattern.Mid(1)))
			{
				bIgnored = false;
			}
		}
		else
		{
			if (MatchesPattern(Normalized, Pattern))
			{
				bIgnored = true;
			}
		}
	}

	return bIgnored;
}

TArray<FString> FAntigravityIgnoreController::FilterPaths(const TArray<FString>& Paths) const
{
	TArray<FString> Result;
	Result.Reserve(Paths.Num());
	for (const FString& Path : Paths)
	{
		if (!IsPathIgnored(Path))
		{
			Result.Add(Path);
		}
	}
	return Result;
}

void FAntigravityIgnoreController::LoadIgnoreFile()
{
	IgnorePatterns.Empty();
	RawContent.Empty();
	bIsLoaded = false;

	const FString IgnorePath = GetIgnoreFilePath();
	if (!FPaths::FileExists(IgnorePath))
	{
		return;
	}

	if (!FFileHelper::LoadFileToString(RawContent, *IgnorePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("AntigravityIgnoreController: Failed to read %s"), *IgnorePath);
		return;
	}

	ParsePatterns(RawContent);
	bIsLoaded = true;

	UE_LOG(LogTemp, Log, TEXT("AntigravityIgnoreController: Loaded %d patterns from .antigravityignore"), IgnorePatterns.Num());
}

void FAntigravityIgnoreController::CreateDefaultIgnoreFile()
{
	const FString IgnorePath = GetIgnoreFilePath();
	const FString DefaultContent = FString(GetDefaultIgnoreContent());

	if (!FFileHelper::SaveStringToFile(DefaultContent, *IgnorePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("AntigravityIgnoreController: Failed to write default .antigravityignore to %s"), *IgnorePath);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AntigravityIgnoreController: Created default .antigravityignore at %s"), *IgnorePath);
	}
}

void FAntigravityIgnoreController::ParsePatterns(const FString& Content)
{
	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines, false);

	for (FString Line : Lines)
	{
		Line.TrimStartAndEndInline();

		// Skip empty lines and comments
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		// Normalize separators to forward slash
		Line.ReplaceInline(TEXT("\\"), TEXT("/"));

		IgnorePatterns.Add(Line);
	}
}

bool FAntigravityIgnoreController::MatchesPattern(const FString& RelativePath, const FString& Pattern) const
{
	// Normalize both
	FString NormPattern = Pattern;
	NormPattern.ReplaceInline(TEXT("\\"), TEXT("/"));

	// Directory pattern (trailing slash) â€” match if path starts with this directory
	if (NormPattern.EndsWith(TEXT("/")))
	{
		const FString DirPrefix = NormPattern.LeftChop(1);
		return RelativePath.StartsWith(DirPrefix + TEXT("/")) || RelativePath == DirPrefix;
	}

	// Simple glob matching: convert to a regex-like pattern
	// Support: * (anything except /), ** (anything including /), ? (single char)
	auto GlobMatch = [](const FString& Text, const FString& Glob) -> bool
	{
		// Simple recursive glob implementation
		int32 ti = 0, gi = 0;
		const int32 tlen = Text.Len(), glen = Glob.Len();

		while (ti < tlen && gi < glen)
		{
			TCHAR g = Glob[gi];

			if (g == TEXT('?'))
			{
				if (Text[ti] == TEXT('/')) return false;
				ti++; gi++;
			}
			else if (g == TEXT('*'))
			{
				// Check for ** (matches anything including /)
				if (gi + 1 < glen && Glob[gi + 1] == TEXT('*'))
				{
					gi += 2;
					if (gi >= glen) return true; // ** at end matches everything
					// Skip the optional separator after **
					if (gi < glen && Glob[gi] == TEXT('/')) gi++;
					// Try matching from every position
					for (int32 i = ti; i <= tlen; i++)
					{
						// Recursive match from position i
						FString Remaining = Text.Mid(i);
						FString GlobRemaining = Glob.Mid(gi);
						// Simple check: see if remaining matches
						if (Remaining == GlobRemaining) return true;
						if (GlobRemaining.IsEmpty()) return Remaining.IsEmpty();
					}
					return false;
				}
				else
				{
					// Single * â€” match anything except /
					gi++;
					while (ti < tlen && Text[ti] != TEXT('/'))
					{
						if (gi >= glen) { ti++; continue; }
						// Try to match rest of pattern with rest of text
						FString TextRem = Text.Mid(ti);
						FString GlobRem = Glob.Mid(gi);
						if (TextRem.StartsWith(GlobRem) || GlobRem.IsEmpty())
						{
							ti += GlobRem.Len();
							gi = glen;
							break;
						}
						ti++;
					}
				}
			}
			else
			{
				if (Text[ti] != g) return false;
				ti++; gi++;
			}
		}

		// Skip remaining * wildcards
		while (gi < glen && Glob[gi] == TEXT('*')) gi++;

		return ti == tlen && gi == glen;
	};

	// If pattern has no slash (except trailing), match against filename only
	// If pattern has slash in the middle, match against full relative path
	const bool bHasSlash = Pattern.Contains(TEXT("/")) && !Pattern.EndsWith(TEXT("/"));

	if (bHasSlash)
	{
		// Match against full path
		return GlobMatch(RelativePath, NormPattern);
	}
	else
	{
		// Match against filename OR full path prefix
		// e.g. "*.uasset" matches "Content/Foo/Bar.uasset"
		const FString FileName = FPaths::GetCleanFilename(RelativePath);
		if (GlobMatch(FileName, NormPattern))
		{
			return true;
		}
		// Also try full path match (for patterns like "Binaries")
		return GlobMatch(RelativePath, NormPattern);
	}
}

FString FAntigravityIgnoreController::NormalizePath(const FString& Path)
{
	FString Result = Path;
	Result.ReplaceInline(TEXT("\\"), TEXT("/"));

	// Remove leading ./
	if (Result.StartsWith(TEXT("./")))
	{
		Result = Result.Mid(2);
	}

	// Remove leading /
	if (Result.StartsWith(TEXT("/")))
	{
		Result = Result.Mid(1);
	}

	return Result;
}
