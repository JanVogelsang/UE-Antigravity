// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityErrorFeedback.h"

FAntigravityErrorFeedback::FAntigravityErrorFeedback() {}
FAntigravityErrorFeedback::~FAntigravityErrorFeedback() {}

FString FAntigravityErrorFeedback::FormatCompilationErrors(const TArray<FString>& Errors)
{
	FString Result = TEXT("Compilation Errors:\n");
	for (const FString& Error : Errors) { Result += FString::Printf(TEXT("- %s\n"), *Error); }
	return Result;
}

FString FAntigravityErrorFeedback::FormatBuildErrors(const FString& BuildOutput) { return FString::Printf(TEXT("Build Output:\n%s"), *BuildOutput); }
bool FAntigravityErrorFeedback::ShouldRetry(const FGuid& ActionId) const { return GetRetryCount(ActionId) < MaxRetries; }
void FAntigravityErrorFeedback::RecordRetry(const FGuid& ActionId) { RetryCountMap.FindOrAdd(ActionId)++; }
void FAntigravityErrorFeedback::ResetRetries(const FGuid& ActionId) { RetryCountMap.Remove(ActionId); }
int32 FAntigravityErrorFeedback::GetRetryCount(const FGuid& ActionId) const { const int32* Count = RetryCountMap.Find(ActionId); return Count ? *Count : 0; }
