// Copyright 2026 AgentFramework. All Rights Reserved.

#include "AgentFrameworkLogCapture.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"

static TSharedPtr<FAgentFrameworkLogCapture> GAgentFrameworkLogCaptureSingleton;
static FCriticalSection GAgentFrameworkLogCaptureInitCS;

FAgentFrameworkLogCapture::FAgentFrameworkLogCapture()
{
}

FAgentFrameworkLogCapture::~FAgentFrameworkLogCapture()
{
	// Device persists for editor session lifetime
}

TSharedPtr<FAgentFrameworkLogCapture> FAgentFrameworkLogCapture::Get()
{
	if (!GAgentFrameworkLogCaptureSingleton.IsValid())
	{
		FScopeLock InitLock(&GAgentFrameworkLogCaptureInitCS);
		if (!GAgentFrameworkLogCaptureSingleton.IsValid())
		{
			GAgentFrameworkLogCaptureSingleton = MakeShared<FAgentFrameworkLogCapture>();
			if (GLog != nullptr)
			{
				GLog->AddOutputDevice(GAgentFrameworkLogCaptureSingleton.Get());
			}
		}
	}
	return GAgentFrameworkLogCaptureSingleton;
}

void FAgentFrameworkLogCapture::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	FScopeLock Lock(&LogLock);

	FAgentFrameworkLogEntry Entry;
	Entry.Category = Category.ToString();
	Entry.Message = V;
	Entry.Verbosity = Verbosity;
	Entry.Time = FPlatformTime::Seconds();

	if (Entries.Num() >= MaxEntries)
	{
		Entries.RemoveAt(0, 1, EAllowShrinking::No);
	}
	Entries.Add(MoveTemp(Entry));
	TotalEntriesEver++;
}

uint64 FAgentFrameworkLogCapture::GetSnapshotIndex()
{
	FScopeLock Lock(&LogLock);
	return TotalEntriesEver;
}

FString FAgentFrameworkLogCapture::GetLogDeltaFormatted(uint64 StartIndex, int32& OutErrorCount, int32& OutWarningCount)
{
	FScopeLock Lock(&LogLock);

	OutErrorCount = 0;
	OutWarningCount = 0;

	uint64 EntriesPassed = (TotalEntriesEver >= StartIndex) ? (TotalEntriesEver - StartIndex) : 0;
	int32 OffsetFromHead = Entries.Num() - static_cast<int32>(FMath::Min<uint64>(EntriesPassed, static_cast<uint64>(Entries.Num())));
	int32 SafeStartIndex = FMath::Clamp(OffsetFromHead, 0, Entries.Num());

	FString DeltaErrors;

	for (int32 i = SafeStartIndex; i < Entries.Num(); ++i)
	{
		const auto& Entry = Entries[i];
		if (Entry.Verbosity == ELogVerbosity::Error || Entry.Verbosity == ELogVerbosity::Fatal)
		{
			OutErrorCount++;
			FString Msg = Entry.Message.Left(300);
			DeltaErrors += FString::Printf(TEXT("  - [%s] %s\n"), *Entry.Category, *Msg);
		}
		else if (Entry.Verbosity == ELogVerbosity::Warning)
		{
			OutWarningCount++;
		}
	}

	return DeltaErrors;
}

void FAgentFrameworkLogCapture::GetEntries(TArray<FAgentFrameworkLogEntry>& OutEntries)
{
	FScopeLock Lock(&LogLock);
	OutEntries = Entries;
}

void FAgentFrameworkLogCapture::ClearEntries()
{
	FScopeLock Lock(&LogLock);
	Entries.Empty();
}
