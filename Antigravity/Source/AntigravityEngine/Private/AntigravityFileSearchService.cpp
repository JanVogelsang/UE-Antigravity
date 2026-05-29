// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityFileSearchService.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Internationalization/Regex.h"

// ============================================================================
// FAntigravitySearchResult
// ============================================================================

FString FAntigravitySearchResult::FormatAsRipgrepOutput() const
{
	if (!bSuccess || Matches.Num() == 0)
	{
		return TEXT("No matches found.");
	}

	FString Output;

	// Group matches by file
	TMap<FString, TArray<const FAntigravitySearchMatch*>> ByFile;
	for (const FAntigravitySearchMatch& Match : Matches)
	{
		ByFile.FindOrAdd(Match.FilePath).Add(&Match);
	}

	for (const auto& FilePair : ByFile)
	{
		Output += FilePair.Key + TEXT("\n");

		for (const FAntigravitySearchMatch* Match : FilePair.Value)
		{
			Output += TEXT("â”‚----\n");

			// Context before
			for (const FString& CtxLine : Match->ContextBefore)
			{
				Output += TEXT("â”‚") + CtxLine + TEXT("\n");
			}

			// The match line (mark with >)
			Output += TEXT("â”‚") + Match->MatchLine + TEXT("\n");

			// Context after
			for (const FString& CtxLine : Match->ContextAfter)
			{
				Output += TEXT("â”‚") + CtxLine + TEXT("\n");
			}

			Output += TEXT("â”‚----\n");
		}

		Output += TEXT("\n");
	}

	Output += FString::Printf(
		TEXT("Found %d match(es) in %d file(s) (searched %d files total)"),
		Matches.Num(), FilesWithMatches, FilesSearched
	);

	return Output;
}

// ============================================================================
// FAntigravityFileSearchService
// ============================================================================

FAntigravityFileSearchService::FAntigravityFileSearchService()
{
}

FAntigravitySearchResult FAntigravityFileSearchService::SearchFiles(
	const FString& ProjectRoot,
	const FString& DirectoryPath,
	const FString& RegexPattern,
	const FString& FilePattern,
	bool bCaseSensitive
) const
{
	FAntigravitySearchResult Result;
	Result.bSuccess = false;

	if (RegexPattern.IsEmpty())
	{
		Result.ErrorMessage = TEXT("Search pattern cannot be empty.");
		return Result;
	}

	// Resolve directory
	FString SearchDir = DirectoryPath;
	if (FPaths::IsRelative(SearchDir))
	{
		SearchDir = FPaths::Combine(ProjectRoot, SearchDir);
	}
	SearchDir = FPaths::ConvertRelativePathToFull(SearchDir);

	if (!FPaths::DirectoryExists(SearchDir))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Directory not found: %s"), *SearchDir);
		return Result;
	}

	// Validate regex pattern
	ERegexPatternFlags PatternFlags = bCaseSensitive
		? ERegexPatternFlags::None
		: ERegexPatternFlags::CaseInsensitive;

	FRegexPattern Pattern(RegexPattern, PatternFlags);

	// Find all files
	TArray<FString> AllFiles;
	IFileManager::Get().FindFilesRecursive(AllFiles, *SearchDir, TEXT("*.*"), true, false);

	int32 MatchCount = 0;

	for (const FString& AbsFilePath : AllFiles)
	{
		if (MatchCount >= MaxMatches)
		{
			break;
		}

		// Check glob pattern
		const FString FileName = FPaths::GetCleanFilename(AbsFilePath);
		if (!FilePattern.IsEmpty() && FilePattern != TEXT("*.*") && FilePattern != TEXT("*"))
		{
			if (!MatchesGlob(FileName, FilePattern))
			{
				continue;
			}
		}

		// Skip binary files
		const FString Ext = FPaths::GetExtension(AbsFilePath).ToLower();
		if (IsBinaryExtension(Ext))
		{
			continue;
		}

		// Skip very large files
		const int64 FileSize = IFileManager::Get().FileSize(*AbsFilePath);
		if (FileSize > (int64)MaxFileSizeKB * 1024)
		{
			continue;
		}

		Result.FilesSearched++;

		// Compute relative path
		FString RelPath = AbsFilePath;
		FString ProjectRootNorm = ProjectRoot;
		FPaths::MakePathRelativeTo(RelPath, *(ProjectRootNorm + TEXT("/")));
		RelPath.ReplaceInline(TEXT("\\"), TEXT("/"));

		TArray<FAntigravitySearchMatch> FileMatches;
		if (SearchSingleFile(AbsFilePath, RelPath, RegexPattern, bCaseSensitive, FileMatches, MatchCount))
		{
			Result.Matches.Append(FileMatches);
			if (FileMatches.Num() > 0)
			{
				Result.FilesWithMatches++;
			}
		}
	}

	Result.bSuccess = true;
	return Result;
}

bool FAntigravityFileSearchService::SearchSingleFile(
	const FString& AbsFilePath,
	const FString& RelFilePath,
	const FString& RegexPattern,
	bool bCaseSensitive,
	TArray<FAntigravitySearchMatch>& OutMatches,
	int32& InOutMatchCount
) const
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *AbsFilePath))
	{
		return false;
	}

	// Split into lines
	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines, false);

	ERegexPatternFlags PatternFlags = bCaseSensitive
		? ERegexPatternFlags::None
		: ERegexPatternFlags::CaseInsensitive;

	FRegexPattern Pattern(RegexPattern, PatternFlags);

	for (int32 i = 0; i < Lines.Num() && InOutMatchCount < MaxMatches; i++)
	{
		const FString& Line = Lines[i];

		FRegexMatcher Matcher(Pattern, Line);
		if (!Matcher.FindNext())
		{
			continue;
		}

		FAntigravitySearchMatch Match;
		Match.FilePath = RelFilePath;
		Match.LineNumber = i + 1;
		Match.MatchLine = Line;

		// Context before (up to ContextLines lines)
		for (int32 j = FMath::Max(0, i - ContextLines); j < i; j++)
		{
			Match.ContextBefore.Add(Lines[j]);
		}

		// Context after (up to ContextLines lines)
		for (int32 j = i + 1; j < FMath::Min(Lines.Num(), i + 1 + ContextLines); j++)
		{
			Match.ContextAfter.Add(Lines[j]);
		}

		OutMatches.Add(Match);
		InOutMatchCount++;
	}

	return true;
}

bool FAntigravityFileSearchService::IsBinaryExtension(const FString& Extension)
{
	static const TSet<FString> BinaryExts = {
		TEXT("uasset"), TEXT("umap"), TEXT("pak"), TEXT("sig"), TEXT("ucas"), TEXT("utoc"),
		TEXT("pdb"), TEXT("exe"), TEXT("dll"), TEXT("lib"), TEXT("obj"), TEXT("bin"),
		TEXT("png"), TEXT("jpg"), TEXT("jpeg"), TEXT("bmp"), TEXT("tga"), TEXT("dds"),
		TEXT("wav"), TEXT("mp3"), TEXT("ogg"), TEXT("fbx"), TEXT("ttf"), TEXT("otf"),
		TEXT("zip"), TEXT("7z"), TEXT("rar"), TEXT("gz")
	};
	return BinaryExts.Contains(Extension);
}

bool FAntigravityFileSearchService::MatchesGlob(const FString& FileName, const FString& GlobPattern)
{
	// Simple glob: support * (any chars) and ? (one char)
	if (GlobPattern == TEXT("*") || GlobPattern == TEXT("*.*"))
	{
		return true;
	}

	// Exact match
	if (FileName.Equals(GlobPattern, ESearchCase::IgnoreCase))
	{
		return true;
	}

	// Extension pattern: *.cpp
	if (GlobPattern.StartsWith(TEXT("*.")))
	{
		const FString Ext = GlobPattern.Mid(1);  // .cpp
		return FileName.EndsWith(Ext, ESearchCase::IgnoreCase);
	}

	// Prefix pattern: MyFile*
	if (GlobPattern.EndsWith(TEXT("*")))
	{
		const FString Prefix = GlobPattern.LeftChop(1);
		return FileName.StartsWith(Prefix, ESearchCase::IgnoreCase);
	}

	// Contains: *Actor*
	if (GlobPattern.StartsWith(TEXT("*")) && GlobPattern.EndsWith(TEXT("*")))
	{
		const FString Middle = GlobPattern.Mid(1, GlobPattern.Len() - 2);
		return FileName.Contains(Middle, ESearchCase::IgnoreCase);
	}

	return false;
}
