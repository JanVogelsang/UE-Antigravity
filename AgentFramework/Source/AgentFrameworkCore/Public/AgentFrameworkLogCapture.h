// Copyright 2026 AgentFramework. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

struct FAgentFrameworkLogEntry
{
	FString Category;
	FString Message;
	ELogVerbosity::Type Verbosity;
	double Time;
};

/** Errors and warnings emitted during a window of log output, plus how many there really were. */
struct FAgentFrameworkLogDelta
{
	/** Error/fatal lines, de-duplicated and capped. */
	TArray<FString> Errors;

	/** Warning lines, de-duplicated and capped, excluding low-signal categories. */
	TArray<FString> Warnings;

	/** Every error/fatal entry in the window, before de-duplication and capping. */
	int32 TotalErrors = 0;

	/** Every warning that passed the category filter, before de-duplication and capping. */
	int32 TotalWarnings = 0;
};

/**
 * Output device that captures log messages into a thread-safe circular buffer.
 * Singleton attached to GLog during module init or first query.
 */
class AGENTFRAMEWORKCORE_API FAgentFrameworkLogCapture : public FOutputDevice
{
public:
	FAgentFrameworkLogCapture();
	virtual ~FAgentFrameworkLogCapture();

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;

	/** Get singleton instance. */
	static TSharedPtr<FAgentFrameworkLogCapture> Get();

	/** Returns total entry count ever recorded (monotonically increasing sequence number for snapshotting). */
	uint64 GetSnapshotIndex();

	/**
	 * Collects the actual text of errors and warnings logged since StartIndex.
	 *
	 * The warning text itself is returned (not just a count) so the router can surface it
	 * to the agent. Without this an agent sees { bSuccess: true } and stays blind to engine
	 * warnings emitted during the tool call.
	 *
	 * Entries are de-duplicated, truncated to 300 chars, and capped at MaxPerSeverity per
	 * severity to keep tool responses small. Low-signal categories are filtered out. The
	 * Total* counts report how many entries were actually eligible, so callers can say how
	 * many were dropped instead of presenting a truncated list as the whole picture.
	 */
	void GetLogDeltaEntries(uint64 StartIndex, FAgentFrameworkLogDelta& OutDelta, int32 MaxPerSeverity = 8);

	/** Thread-safely copies captured entries into OutEntries. */
	void GetEntries(TArray<FAgentFrameworkLogEntry>& OutEntries);

	/** Thread-safely clears all log entries. */
	void ClearEntries();

private:
	TArray<FAgentFrameworkLogEntry> Entries;
	int32 MaxEntries = 500;
	uint64 TotalEntriesEver = 0;
	FCriticalSection LogLock;
};
