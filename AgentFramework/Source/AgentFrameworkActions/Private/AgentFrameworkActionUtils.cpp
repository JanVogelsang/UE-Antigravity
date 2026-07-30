// Copyright 2026 AgentFramework. All Rights Reserved.

#include "AgentFrameworkActionUtils.h"
#include "UObject/SavePackage.h"
#include "Misc/ScopeLock.h"
#include "HAL/CriticalSection.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ============================================================================
// Internal Telemetry Storage & Thread Safety
// ============================================================================

namespace AgentFrameworkTelemetryInternal
{
	static FCriticalSection TelemetryCS;
	static TMap<FString, FAgentFrameworkToolMetrics> ToolMetricsMap;
	static TArray<FAgentFrameworkErrorRecord> ErrorRingBuffer;
	static constexpr int32 MaxErrorRingBufferCapacity = 256;
}

// ============================================================================
// Parameter Parsing Utilities
// ============================================================================

bool UAgentFrameworkActionUtils::TryGetStringParam(const TSharedPtr<FJsonObject>& InParams, const FString& InFieldName, FString& OutValue, TArray<FString>& OutErrors, bool bRequired)
{
	if (!InParams.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("JSON parameters object is invalid (null) when looking for field: %s"), *InFieldName));
		return false;
	}

	if (!InParams->TryGetStringField(InFieldName, OutValue))
	{
		if (bRequired)
		{
			OutErrors.Add(FString::Printf(TEXT("Parameter '%s' is required."), *InFieldName));
			return false;
		}
		return true;
	}

	if (bRequired && OutValue.IsEmpty())
	{
		OutErrors.Add(FString::Printf(TEXT("Parameter '%s' is required and must not be empty."), *InFieldName));
		return false;
	}

	return true;
}

bool UAgentFrameworkActionUtils::TryGetBoolParam(const TSharedPtr<FJsonObject>& InParams, const FString& InFieldName, bool& OutValue, TArray<FString>& OutErrors, bool bRequired)
{
	if (!InParams.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("JSON parameters object is invalid (null) when looking for field: %s"), *InFieldName));
		return false;
	}

	if (!InParams->TryGetBoolField(InFieldName, OutValue))
	{
		if (bRequired)
		{
			OutErrors.Add(FString::Printf(TEXT("Parameter '%s' is required."), *InFieldName));
			return false;
		}
	}

	return true;
}

bool UAgentFrameworkActionUtils::TryGetDoubleParam(const TSharedPtr<FJsonObject>& InParams, const FString& InFieldName, double& OutValue, TArray<FString>& OutErrors, bool bRequired)
{
	if (!InParams.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("JSON parameters object is invalid (null) when looking for field: %s"), *InFieldName));
		return false;
	}

	if (!InParams->TryGetNumberField(InFieldName, OutValue))
	{
		if (bRequired)
		{
			OutErrors.Add(FString::Printf(TEXT("Parameter '%s' is required and must be a number."), *InFieldName));
			return false;
		}
	}

	return true;
}

bool UAgentFrameworkActionUtils::TryGetFloatParam(const TSharedPtr<FJsonObject>& InParams, const FString& InFieldName, float& OutValue, TArray<FString>& OutErrors, bool bRequired)
{
	double TempDouble = static_cast<double>(OutValue);
	if (!TryGetDoubleParam(InParams, InFieldName, TempDouble, OutErrors, bRequired))
	{
		return false;
	}
	OutValue = static_cast<float>(TempDouble);
	return true;
}

bool UAgentFrameworkActionUtils::TryGetIntParam(const TSharedPtr<FJsonObject>& InParams, const FString& InFieldName, int32& OutValue, TArray<FString>& OutErrors, bool bRequired)
{
	double TempDouble = static_cast<double>(OutValue);
	if (!TryGetDoubleParam(InParams, InFieldName, TempDouble, OutErrors, bRequired))
	{
		return false;
	}
	OutValue = static_cast<int32>(TempDouble);
	return true;
}

bool UAgentFrameworkActionUtils::TryGetStringArrayParam(const TSharedPtr<FJsonObject>& InParams, const FString& InFieldName, TArray<FString>& OutValue, TArray<FString>& OutErrors, bool bRequired)
{
	if (!InParams.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("JSON parameters object is invalid (null) when looking for field: %s"), *InFieldName));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ArrayField = nullptr;
	if (!InParams->TryGetArrayField(InFieldName, ArrayField))
	{
		if (bRequired)
		{
			OutErrors.Add(FString::Printf(TEXT("Parameter '%s' is required and must be an array."), *InFieldName));
			return false;
		}
		return true;
	}

	if (bRequired && ArrayField->Num() == 0)
	{
		OutErrors.Add(FString::Printf(TEXT("Parameter '%s' must not be empty."), *InFieldName));
		return false;
	}

	for (int32 Index = 0; Index < ArrayField->Num(); ++Index)
	{
		FString Val;
		if ((*ArrayField)[Index]->TryGetString(Val))
		{
			OutValue.Add(Val);
		}
		else
		{
			OutErrors.Add(FString::Printf(TEXT("Element at index %d in array '%s' is not a string."), Index, *InFieldName));
			return false;
		}
	}

	return true;
}

bool UAgentFrameworkActionUtils::TryGetObjectParam(const TSharedPtr<FJsonObject>& InParams, const FString& InFieldName, const TSharedPtr<FJsonObject>*& OutValue, TArray<FString>& OutErrors, bool bRequired)
{
	if (!InParams.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("JSON parameters object is invalid (null) when looking for field: %s"), *InFieldName));
		return false;
	}

	if (!InParams->TryGetObjectField(InFieldName, OutValue))
	{
		if (bRequired)
		{
			OutErrors.Add(FString::Printf(TEXT("Parameter '%s' is required and must be an object."), *InFieldName));
			return false;
		}
	}

	return true;
}

bool UAgentFrameworkActionUtils::TryGetArrayParam(const TSharedPtr<FJsonObject>& InParams, const FString& InFieldName, const TArray<TSharedPtr<FJsonValue>>*& OutValue, TArray<FString>& OutErrors, bool bRequired)
{
	if (!InParams.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("JSON parameters object is invalid (null) when looking for field: %s"), *InFieldName));
		return false;
	}

	if (!InParams->TryGetArrayField(InFieldName, OutValue))
	{
		if (bRequired)
		{
			OutErrors.Add(FString::Printf(TEXT("Parameter '%s' is required and must be an array."), *InFieldName));
			return false;
		}
	}

	return true;
}

// ============================================================================
// Telemetry & Diagnostic Memory Implementation
// ============================================================================

void UAgentFrameworkActionUtils::RecordToolExecution(const FString& ToolName, double DurationMicros, bool bSuccess, const TArray<FString>& Errors, const FString& ContextSummary)
{
	FScopeLock Lock(&AgentFrameworkTelemetryInternal::TelemetryCS);

	FAgentFrameworkToolMetrics& Metrics = AgentFrameworkTelemetryInternal::ToolMetricsMap.FindOrAdd(ToolName);
	if (Metrics.TotalExecutions == 0)
	{
		Metrics.ToolName = ToolName;
		Metrics.TotalExecutions = 1;
		Metrics.SuccessCount = bSuccess ? 1 : 0;
		Metrics.ErrorCount = bSuccess ? 0 : 1;
		Metrics.TotalDurationMicros = DurationMicros;
		Metrics.MinDurationMicros = DurationMicros;
		Metrics.MaxDurationMicros = DurationMicros;
		Metrics.AvgDurationMicros = DurationMicros;
		Metrics.LastExecutionTime = FDateTime::UtcNow();
		Metrics.bLastSuccess = bSuccess;
	}
	else
	{
		Metrics.TotalExecutions++;
		if (bSuccess)
		{
			Metrics.SuccessCount++;
		}
		else
		{
			Metrics.ErrorCount++;
		}
		Metrics.TotalDurationMicros += DurationMicros;
		if (DurationMicros < Metrics.MinDurationMicros)
		{
			Metrics.MinDurationMicros = DurationMicros;
		}
		if (DurationMicros > Metrics.MaxDurationMicros)
		{
			Metrics.MaxDurationMicros = DurationMicros;
		}
		Metrics.AvgDurationMicros = Metrics.TotalDurationMicros / static_cast<double>(Metrics.TotalExecutions);
		Metrics.LastExecutionTime = FDateTime::UtcNow();
		Metrics.bLastSuccess = bSuccess;
	}

	if (!bSuccess || Errors.Num() > 0)
	{
		for (const FString& ErrMsg : Errors)
		{
			if (ErrMsg.IsEmpty())
			{
				continue;
			}

			FAgentFrameworkErrorRecord* ExistingRecord = nullptr;
			for (FAgentFrameworkErrorRecord& Record : AgentFrameworkTelemetryInternal::ErrorRingBuffer)
			{
				if (Record.ToolName == ToolName && Record.ErrorMessage == ErrMsg)
				{
					ExistingRecord = &Record;
					break;
				}
			}

			if (ExistingRecord)
			{
				ExistingRecord->Frequency++;
				ExistingRecord->Timestamp = FDateTime::UtcNow();
				if (!ContextSummary.IsEmpty())
				{
					ExistingRecord->ContextSummary = ContextSummary;
				}
			}
			else
			{
				if (AgentFrameworkTelemetryInternal::ErrorRingBuffer.Num() >= AgentFrameworkTelemetryInternal::MaxErrorRingBufferCapacity)
				{
					AgentFrameworkTelemetryInternal::ErrorRingBuffer.RemoveAt(0);
				}

				FAgentFrameworkErrorRecord NewRecord;
				NewRecord.ToolName = ToolName;
				NewRecord.ErrorMessage = ErrMsg;
				NewRecord.Timestamp = FDateTime::UtcNow();
				NewRecord.Frequency = 1;
				NewRecord.ContextSummary = ContextSummary;

				AgentFrameworkTelemetryInternal::ErrorRingBuffer.Add(NewRecord);
			}
		}
	}
}

void UAgentFrameworkActionUtils::RecordToolExecution(const FString& ToolName, double DurationMicros, bool bSuccess, const TArray<FString>& Errors, TSharedPtr<FJsonObject> Context)
{
	FString ContextSummary;
	if (Context.IsValid())
	{
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ContextSummary);
		FJsonSerializer::Serialize(Context.ToSharedRef(), Writer);
		if (ContextSummary.Len() > 256)
		{
			ContextSummary = ContextSummary.Left(256) + TEXT("...");
		}
	}
	RecordToolExecution(ToolName, DurationMicros, bSuccess, Errors, ContextSummary);
}

TArray<FAgentFrameworkToolMetrics> UAgentFrameworkActionUtils::GetToolTelemetry(const FString& ToolNameFilter)
{
	FScopeLock Lock(&AgentFrameworkTelemetryInternal::TelemetryCS);
	TArray<FAgentFrameworkToolMetrics> Results;

	for (const auto& Pair : AgentFrameworkTelemetryInternal::ToolMetricsMap)
	{
		if (ToolNameFilter.IsEmpty() ||
			Pair.Key.Equals(ToolNameFilter, ESearchCase::IgnoreCase) ||
			Pair.Key.Contains(ToolNameFilter, ESearchCase::IgnoreCase))
		{
			Results.Add(Pair.Value);
		}
	}

	return Results;
}

TArray<FAgentFrameworkErrorRecord> UAgentFrameworkActionUtils::GetRecentErrors(int32 MaxCount, const FString& ToolNameFilter)
{
	FScopeLock Lock(&AgentFrameworkTelemetryInternal::TelemetryCS);
	TArray<FAgentFrameworkErrorRecord> Results;

	for (int32 Index = AgentFrameworkTelemetryInternal::ErrorRingBuffer.Num() - 1; Index >= 0; --Index)
	{
		const FAgentFrameworkErrorRecord& Record = AgentFrameworkTelemetryInternal::ErrorRingBuffer[Index];
		if (ToolNameFilter.IsEmpty() ||
			Record.ToolName.Equals(ToolNameFilter, ESearchCase::IgnoreCase) ||
			Record.ToolName.Contains(ToolNameFilter, ESearchCase::IgnoreCase))
		{
			Results.Add(Record);
			if (Results.Num() >= MaxCount)
			{
				break;
			}
		}
	}

	return Results;
}

FString UAgentFrameworkActionUtils::GetTelemetryMetricsJson()
{
	FScopeLock Lock(&AgentFrameworkTelemetryInternal::TelemetryCS);

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	int64 TotalExecutions = 0;
	int64 TotalSuccesses = 0;
	int64 TotalErrors = 0;

	TArray<TSharedPtr<FJsonValue>> MetricsArray;
	for (const auto& Pair : AgentFrameworkTelemetryInternal::ToolMetricsMap)
	{
		const FAgentFrameworkToolMetrics& M = Pair.Value;
		TotalExecutions += M.TotalExecutions;
		TotalSuccesses += M.SuccessCount;
		TotalErrors += M.ErrorCount;

		TSharedRef<FJsonObject> MetricObj = MakeShared<FJsonObject>();
		MetricObj->SetStringField(TEXT("tool_name"), M.ToolName);
		MetricObj->SetNumberField(TEXT("total_executions"), M.TotalExecutions);
		MetricObj->SetNumberField(TEXT("success_count"), M.SuccessCount);
		MetricObj->SetNumberField(TEXT("error_count"), M.ErrorCount);
		MetricObj->SetNumberField(TEXT("total_duration_micros"), M.TotalDurationMicros);
		MetricObj->SetNumberField(TEXT("min_duration_micros"), M.MinDurationMicros);
		MetricObj->SetNumberField(TEXT("max_duration_micros"), M.MaxDurationMicros);
		MetricObj->SetNumberField(TEXT("avg_duration_micros"), M.AvgDurationMicros);
		MetricObj->SetStringField(TEXT("last_execution_time"), M.LastExecutionTime.ToIso8601());
		MetricObj->SetBoolField(TEXT("last_success"), M.bLastSuccess);

		MetricsArray.Add(MakeShared<FJsonValueObject>(MetricObj));
	}

	RootObject->SetNumberField(TEXT("total_tools_tracked"), AgentFrameworkTelemetryInternal::ToolMetricsMap.Num());
	RootObject->SetNumberField(TEXT("total_executions"), TotalExecutions);
	RootObject->SetNumberField(TEXT("total_successes"), TotalSuccesses);
	RootObject->SetNumberField(TEXT("total_errors"), TotalErrors);
	RootObject->SetArrayField(TEXT("metrics"), MetricsArray);

	TArray<TSharedPtr<FJsonValue>> ErrorsArray;
	for (int32 Index = AgentFrameworkTelemetryInternal::ErrorRingBuffer.Num() - 1; Index >= 0; --Index)
	{
		const FAgentFrameworkErrorRecord& E = AgentFrameworkTelemetryInternal::ErrorRingBuffer[Index];
		TSharedRef<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetStringField(TEXT("tool_name"), E.ToolName);
		ErrorObj->SetStringField(TEXT("error_message"), E.ErrorMessage);
		ErrorObj->SetStringField(TEXT("timestamp"), E.Timestamp.ToIso8601());
		ErrorObj->SetNumberField(TEXT("frequency"), E.Frequency);
		ErrorObj->SetStringField(TEXT("context"), E.ContextSummary);

		ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrorObj));
	}
	RootObject->SetArrayField(TEXT("recent_errors"), ErrorsArray);

	FString OutputJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputJson);
	FJsonSerializer::Serialize(RootObject, Writer);

	return OutputJson;
}

void UAgentFrameworkActionUtils::ClearTelemetryData()
{
	FScopeLock Lock(&AgentFrameworkTelemetryInternal::TelemetryCS);
	AgentFrameworkTelemetryInternal::ToolMetricsMap.Empty();
	AgentFrameworkTelemetryInternal::ErrorRingBuffer.Empty();
}

// ============================================================================
// FAgentFrameworkScopedTelemetry Profiler
// ============================================================================

FAgentFrameworkScopedTelemetry::FAgentFrameworkScopedTelemetry(const FString& InToolName, TSharedPtr<FJsonObject> InContext)
	: ToolName(InToolName)
	, StartTimeSeconds(FPlatformTime::Seconds())
	, bSuccess(false)
	, bResultSet(false)
{
	if (InContext.IsValid())
	{
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ContextSummary);
		FJsonSerializer::Serialize(InContext.ToSharedRef(), Writer);
		if (ContextSummary.Len() > 256)
		{
			ContextSummary = ContextSummary.Left(256) + TEXT("...");
		}
	}
}

FAgentFrameworkScopedTelemetry::FAgentFrameworkScopedTelemetry(const FString& InToolName, const FString& InContextSummary)
	: ToolName(InToolName)
	, ContextSummary(InContextSummary)
	, StartTimeSeconds(FPlatformTime::Seconds())
	, bSuccess(false)
	, bResultSet(false)
{
}

FAgentFrameworkScopedTelemetry::~FAgentFrameworkScopedTelemetry()
{
	double DurationMicros = (FPlatformTime::Seconds() - StartTimeSeconds) * 1000000.0;
	if (DurationMicros < 0.0)
	{
		DurationMicros = 0.0;
	}
	UAgentFrameworkActionUtils::RecordToolExecution(ToolName, DurationMicros, bSuccess, Errors, ContextSummary);
}

void FAgentFrameworkScopedTelemetry::SetResult(bool bInSuccess, const TArray<FString>& InErrors)
{
	bSuccess = bInSuccess;
	Errors = InErrors;
	bResultSet = true;
}

bool UAgentFrameworkActionUtils::SaveAssetPackage(UPackage* Package, UObject* Asset, const FString& PackageFileName)
{
	if (!Package || !Asset || PackageFileName.IsEmpty())
	{
		return false;
	}

	Package->FullyLoad();

	FSavePackageArgs SaveArgs;
	return UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
}

FString UAgentFrameworkActionUtils::NormalizeAssetObjectPath(const FString& InPath)
{
	if (InPath.IsEmpty())
	{
		return InPath;
	}

	int32 LastDotIndex = INDEX_NONE;
	int32 LastSlashIndex = INDEX_NONE;
	InPath.FindLastChar(TEXT('.'), LastDotIndex);
	InPath.FindLastChar(TEXT('/'), LastSlashIndex);

	if (LastDotIndex != INDEX_NONE && LastDotIndex > LastSlashIndex)
	{
		return InPath;
	}

	FString AssetName = FPackageName::GetLongPackageAssetName(InPath);
	if (!AssetName.IsEmpty())
	{
		return FString::Printf(TEXT("%s.%s"), *InPath, *AssetName);
	}

	return InPath;
}

void UAgentFrameworkActionUtils::SplitAssetPath(const FString& InPath, FString& OutPackageName, FString& OutPackagePath, FString& OutAssetName)
{
	OutPackageName = InPath;
	OutPackagePath.Reset();
	OutAssetName.Reset();

	if (InPath.IsEmpty())
	{
		return;
	}

	// A trailing slash would otherwise yield an empty asset name and break suffix detection.
	while (OutPackageName.Len() > 1 && OutPackageName.EndsWith(TEXT("/")))
	{
		OutPackageName = OutPackageName.LeftChop(1);
	}

	// Strip a trailing ".ObjectName" suffix, but only when the dot comes after the last
	// slash — a dot earlier in the path is part of a folder name, not an object suffix.
	int32 LastSlashIndex = INDEX_NONE;
	int32 LastDotIndex = INDEX_NONE;
	OutPackageName.FindLastChar(TEXT('/'), LastSlashIndex);
	OutPackageName.FindLastChar(TEXT('.'), LastDotIndex);
	if (LastDotIndex != INDEX_NONE && LastDotIndex > LastSlashIndex)
	{
		OutPackageName = OutPackageName.Left(LastDotIndex);
	}

	OutPackagePath = FPackageName::GetLongPackagePath(OutPackageName);
	OutAssetName = FPackageName::GetLongPackageAssetName(OutPackageName);
}

