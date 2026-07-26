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

	/** Formats log errors and counts warnings logged since StartIndex. */
	FString GetLogDeltaFormatted(uint64 StartIndex, int32& OutErrorCount, int32& OutWarningCount);

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
