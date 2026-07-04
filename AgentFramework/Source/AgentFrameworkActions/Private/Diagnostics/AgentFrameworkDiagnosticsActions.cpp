// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Diagnostics/AgentFrameworkDiagnosticsActions.h"
#include "AgentFrameworkCoreModule.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/OutputDeviceHelper.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "AgentFrameworkDiagnosticsActions"

/**
 * Custom output device that captures log messages into a buffer.
 * Attached temporarily during read_message_log to capture recent entries.
 */
class FAgentFrameworkLogCapture : public FOutputDevice
{
public:
	struct FLogEntry
	{
		FString Category;
		FString Message;
		ELogVerbosity::Type Verbosity;
		double Time;
	};

	TArray<FLogEntry> Entries;
	int32 MaxEntries = 500;

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		FLogEntry Entry;
		Entry.Category = Category.ToString();
		Entry.Message = V;
		Entry.Verbosity = Verbosity;
		Entry.Time = FPlatformTime::Seconds();

		if (Entries.Num() >= MaxEntries)
		{
			Entries.RemoveAt(0, 1, EAllowShrinking::No);
		}
		Entries.Add(MoveTemp(Entry));
	}
};

// Static log capture instance â€” always listening
static TSharedPtr<FAgentFrameworkLogCapture> GAgentFrameworkLogCapture;

// ============================================================================
// Lifecycle
// ============================================================================

FAgentFrameworkDiagnosticsActions::FAgentFrameworkDiagnosticsActions()
{
	// Create and attach the log capture device so it accumulates messages
	if (!GAgentFrameworkLogCapture.IsValid())
	{
		GAgentFrameworkLogCapture = MakeShared<FAgentFrameworkLogCapture>();
		GLog->AddOutputDevice(GAgentFrameworkLogCapture.Get());
	}
}

FAgentFrameworkDiagnosticsActions::~FAgentFrameworkDiagnosticsActions()
{
	// We intentionally do NOT remove the device here â€” it's a singleton
	// that persists for the editor session.
}

// ============================================================================
// IAgentFrameworkActionExecutor Interface
// ============================================================================

FName FAgentFrameworkDiagnosticsActions::GetActionName() const { return FName(TEXT("Diagnostics")); }

TArray<FString> FAgentFrameworkDiagnosticsActions::GetSupportedToolNames() const
{
	return { TEXT("read_message_log"), TEXT("shutdown_editor") };
}

bool FAgentFrameworkDiagnosticsActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true; // All params optional
}

FAgentFrameworkActionResult FAgentFrameworkDiagnosticsActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FString ToolName;
	Params->TryGetStringField(TEXT("_tool_name"), ToolName);

	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	if (ToolName == TEXT("read_message_log"))
	{
		return ExecuteReadMessageLog(Params, Result);
	}
	else if (ToolName == TEXT("shutdown_editor"))
	{
		return ExecuteShutdownEditor(Params, Result);
	}

	Result.Errors.Add(FString::Printf(TEXT("Unknown tool name: %s"), *ToolName));
	return Result;
}

// ============================================================================
// read_message_log
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkDiagnosticsActions::ExecuteReadMessageLog(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	if (!GAgentFrameworkLogCapture.IsValid())
	{
		Result.Errors.Add(TEXT("Log capture device is not initialized."));
		return Result;
	}

	// Parse optional filters
	int32 MaxLines = 100;
	Params->TryGetNumberField(TEXT("max_lines"), MaxLines);
	MaxLines = FMath::Clamp(MaxLines, 1, 500);

	FString CategoryFilter;
	Params->TryGetStringField(TEXT("category_filter"), CategoryFilter);

	FString SeverityFilter;
	Params->TryGetStringField(TEXT("severity_filter"), SeverityFilter);

	bool bErrorsOnly = false;
	if (SeverityFilter.Equals(TEXT("Error"), ESearchCase::IgnoreCase))
		bErrorsOnly = true;

	bool bWarningsAndErrors = false;
	if (SeverityFilter.Equals(TEXT("Warning"), ESearchCase::IgnoreCase))
		bWarningsAndErrors = true;

	// Filter and format entries
	FString Output = TEXT("=== Output Log ===\n");
	int32 OutputCount = 0;
	int32 TotalErrors = 0;
	int32 TotalWarnings = 0;

	// Read from end (most recent first)
	const auto& Entries = GAgentFrameworkLogCapture->Entries;
	int32 StartIdx = FMath::Max(0, Entries.Num() - MaxLines * 2); // Over-read to account for filtering

	for (int32 i = Entries.Num() - 1; i >= StartIdx && OutputCount < MaxLines; --i)
	{
		const auto& Entry = Entries[i];

		// Count stats
		if (Entry.Verbosity == ELogVerbosity::Error || Entry.Verbosity == ELogVerbosity::Fatal)
			TotalErrors++;
		if (Entry.Verbosity == ELogVerbosity::Warning)
			TotalWarnings++;

		// Apply category filter
		if (!CategoryFilter.IsEmpty() && !Entry.Category.Contains(CategoryFilter, ESearchCase::IgnoreCase))
			continue;

		// Apply severity filter
		if (bErrorsOnly && Entry.Verbosity != ELogVerbosity::Error && Entry.Verbosity != ELogVerbosity::Fatal)
			continue;
		if (bWarningsAndErrors && Entry.Verbosity != ELogVerbosity::Error
			&& Entry.Verbosity != ELogVerbosity::Fatal && Entry.Verbosity != ELogVerbosity::Warning)
			continue;

		// Format the entry
		FString Severity;
		switch (Entry.Verbosity)
		{
		case ELogVerbosity::Fatal:   Severity = TEXT("FATAL"); break;
		case ELogVerbosity::Error:   Severity = TEXT("ERROR"); break;
		case ELogVerbosity::Warning: Severity = TEXT("WARN "); break;
		default:                     Severity = TEXT("LOG  "); break;
		}

		// Truncate very long messages
		FString Msg = Entry.Message.Left(500);
		Output += FString::Printf(TEXT("[%s] %s: %s\n"), *Severity, *Entry.Category, *Msg);
		OutputCount++;
	}

	if (OutputCount == 0)
	{
		Output += TEXT("  (no matching log entries found)\n");
	}

	Output += FString::Printf(TEXT("\n--- Showing %d of %d total entries | %d errors, %d warnings ---\n"),
		OutputCount, Entries.Num(), TotalErrors, TotalWarnings);

	// Optionally clear after reading
	bool bClearAfterRead = false;
	Params->TryGetBoolField(TEXT("clear_after_read"), bClearAfterRead);
	if (bClearAfterRead)
	{
		GAgentFrameworkLogCapture->Entries.Empty();
		Output += TEXT("(log buffer cleared)\n");
	}

	Result.bSuccess = true;
	Result.ResultMessage = Output;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkDiagnosticsActions::ExecuteShutdownEditor(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	AsyncTask(ENamedThreads::GameThread, []() {
		FPlatformMisc::RequestExit(false);
	});

	Result.bSuccess = true;
	Result.ResultMessage = TEXT("Editor shutdown initiated successfully.");
	return Result;
}

#undef LOCTEXT_NAMESPACE
