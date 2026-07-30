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

namespace AgentFrameworkLogCaptureInternal
{
	/**
	 * Categories that emit high-volume warnings unrelated to whatever tool is executing
	 * (renderer/shader/audio churn, HTTP server chatter). Surfacing these to the agent
	 * would bury the signal, so they are dropped from the warning delta. Errors are
	 * never filtered — an error in any category is worth reporting.
	 */
	static bool IsLowSignalWarningCategory(const FString& Category)
	{
		static const TCHAR* DeniedCategories[] = {
			TEXT("LogSlate"),
			TEXT("LogRHI"),
			TEXT("LogD3D12RHI"),
			TEXT("LogShaderCompilers"),
			TEXT("LogShaders"),
			TEXT("LogAudioMixer"),
			TEXT("LogHttp"),
			TEXT("LogHttpServer"),
			TEXT("LogDerivedDataCache"),
			TEXT("LogVirtualization")
		};

		for (const TCHAR* Denied : DeniedCategories)
		{
			if (Category.Equals(Denied, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}

void FAgentFrameworkLogCapture::GetLogDeltaEntries(uint64 StartIndex, FAgentFrameworkLogDelta& OutDelta, int32 MaxPerSeverity)
{
	FScopeLock Lock(&LogLock);

	OutDelta.Errors.Reset();
	OutDelta.Warnings.Reset();
	OutDelta.TotalErrors = 0;
	OutDelta.TotalWarnings = 0;

	uint64 EntriesPassed = (TotalEntriesEver >= StartIndex) ? (TotalEntriesEver - StartIndex) : 0;
	int32 OffsetFromHead = Entries.Num() - static_cast<int32>(FMath::Min<uint64>(EntriesPassed, static_cast<uint64>(Entries.Num())));
	int32 SafeStartIndex = FMath::Clamp(OffsetFromHead, 0, Entries.Num());

	for (int32 i = SafeStartIndex; i < Entries.Num(); ++i)
	{
		const FAgentFrameworkLogEntry& Entry = Entries[i];

		const bool bIsError = (Entry.Verbosity == ELogVerbosity::Error || Entry.Verbosity == ELogVerbosity::Fatal);
		const bool bIsWarning = (Entry.Verbosity == ELogVerbosity::Warning);
		if (!bIsError && !bIsWarning)
		{
			continue;
		}

		if (bIsWarning && AgentFrameworkLogCaptureInternal::IsLowSignalWarningCategory(Entry.Category))
		{
			continue;
		}

		// Count every eligible entry, even the ones the cap drops, so the caller can report
		// how much it is not showing rather than passing off a truncated list as complete.
		TArray<FString>& Target = bIsError ? OutDelta.Errors : OutDelta.Warnings;
		int32& Total = bIsError ? OutDelta.TotalErrors : OutDelta.TotalWarnings;
		++Total;

		if (MaxPerSeverity <= 0 || Target.Num() >= MaxPerSeverity)
		{
			continue;
		}

		FString Formatted = FString::Printf(TEXT("[%s] %s"), *Entry.Category, *Entry.Message.Left(300));
		Target.AddUnique(MoveTemp(Formatted));
	}
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
