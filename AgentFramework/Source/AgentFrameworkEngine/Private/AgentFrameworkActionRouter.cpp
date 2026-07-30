// Copyright 2026 AgentFramework. All Rights Reserved.

#include "AgentFrameworkActionRouter.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkLogCapture.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeLock.h"
#include "Async/Async.h"
#include "Templates/UnrealTemplate.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <excpt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

FOnToolExecutionRecorded FAgentFrameworkActionRouter::OnToolExecutionRecorded;

FAgentFrameworkActionRouter::FAgentFrameworkActionRouter() {}

FAgentFrameworkActionRouter::~FAgentFrameworkActionRouter()
{
	ClearPendingTasks();
}

void FAgentFrameworkActionRouter::RegisterExecutor(TSharedRef<IAgentFrameworkActionExecutor> Executor)
{
	for (const FString& ToolName : Executor->GetSupportedToolNames())
	{
		ExecutorMap.Add(ToolName, Executor);
		UE_LOG(LogAgentFramework, Log, TEXT("ActionRouter: Registered executor for tool '%s'"), *ToolName);
	}
}

void FAgentFrameworkActionRouter::ClearExecutors()
{
	ExecutorMap.Empty();
}

// ============================================================================
// Centralized asset_path validation
// ============================================================================
// Many tools accept an "asset_path" parameter (e.g. /Game/UI/WBP_MainMenu).
// If the LLM sends an invalid path (e.g. "/", "", "MainMenu"), UE's native
// CreatePackage() / FPackageName::LongPackageNameToFilename() will trigger a
// FMessageDialog::Open() â€” a MODAL dialog that freezes the editor with:
//   "Input '/' contains fewer than the minimum number of characters 4 for LongPackageNames"
//
// We intercept this here BEFORE dispatching to any executor, using
// FPackageName::IsValidLongPackageName() which is SAFE (returns bool + reason,
// no dialog). This prevents the modal freeze and returns a clear error to the
// LLM so it can retry with a correct path.
// ============================================================================

static bool ValidateAssetPathParam(const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
	if (!Params.IsValid()) return true; // No params, nothing to validate

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return true; // No asset_path param â€” tool doesn't need one
	}

	// Empty path
	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("asset_path is empty. You must provide a valid Unreal asset path starting with /Game/. Example: /Game/UI/WBP_MainMenu");
		return false;
	}

	// Must start with /Game/
	if (!AssetPath.StartsWith(TEXT("/Game/")))
	{
		OutError = FString::Printf(
			TEXT("asset_path '%s' is invalid â€” it must start with /Game/. Example: /Game/UI/WBP_MainMenu or /Game/Blueprints/BP_MyActor"),
			*AssetPath);
		return false;
	}

	// Strip the optional ".ObjectName" suffix from full object references.
	// UE5 has two asset reference formats:
	//   1. Package path:        /Game/Blueprints/BP_Character
	//   2. Full object ref:     /Game/Blueprints/BP_Character.BP_Character
	// FPackageName::IsValidLongPackageName() validates PACKAGE names only and
	// rejects the '.' in format #2. The AI model often sends format #2, so we
	// strip the suffix for validation purposes. The original path (with dot) is
	// preserved in the params â€” LoadObject() accepts both formats.
	FString PackagePath = AssetPath;
	int32 LastSlash = INDEX_NONE;
	PackagePath.FindLastChar(TEXT('/'), LastSlash);
	if (LastSlash != INDEX_NONE)
	{
		int32 DotIndex = PackagePath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, LastSlash);
		if (DotIndex != INDEX_NONE)
		{
			PackagePath = PackagePath.Left(DotIndex);
		}
	}

	// Use UE's built-in validation (safe â€” no dialog, just returns bool + reason)
	FText Reason;
	if (!FPackageName::IsValidLongPackageName(PackagePath, false, &Reason))
	{
		OutError = FString::Printf(
			TEXT("asset_path '%s' is not a valid Unreal package path: %s. Use a path like /Game/UI/WBP_MainMenu"),
			*AssetPath, *Reason.ToString());
		return false;
	}

	return true;
}

// ============================================================================
// Diagnostic Enrichment
// ============================================================================

struct FAgentFrameworkDiagnostics
{
	/**
	 * True when ResultMessage carries a machine-parsed payload rather than prose.
	 *
	 * Tools like extract_ui_state and get_blueprint_info return a JSON document in
	 * ResultMessage, and the capture_* tools return base64 image data; callers parse both
	 * verbatim. Appending a diagnostics block to those would corrupt the payload, so for
	 * them the Warnings array is the only safe channel.
	 */
	static bool IsOpaquePayload(const FString& Message)
	{
		const FString Trimmed = Message.TrimStart();
		if (Trimmed.IsEmpty())
		{
			return false;
		}
		if (Trimmed.StartsWith(TEXT("{")) || Trimmed.StartsWith(TEXT("[")))
		{
			return true;
		}
		if (Trimmed.StartsWith(TEXT("data:")))
		{
			return true;
		}
		// A long run with no whitespace is a raw base64 blob, not a message.
		return Trimmed.Len() > 512 && !Trimmed.Contains(TEXT(" "));
	}

	static void EnrichErrorString(FString& ErrorMsg)
	{
		if (ErrorMsg.Contains(TEXT("Failed to load package")) || ErrorMsg.Contains(TEXT("LoadObject failed")))
		{
			ErrorMsg += TEXT("\n[AI HINT: The asset path does not exist. Verify you are using the internal '/Game/...' path format, NOT a Windows filesystem path. Use search_assets with the file basename to discover the correct path.]");
		}
		else if (ErrorMsg.Contains(TEXT("Could not find class")) || ErrorMsg.Contains(TEXT("Class not found")))
		{
			ErrorMsg += TEXT("\n[AI HINT: The class was not found. If this is a C++ class, ensure it has UCLASS(BlueprintType/Blueprintable) and that you have compiled the module. If it is a Blueprint, ensure you have the correct path.]");
		}
		else if (ErrorMsg.Contains(TEXT("Could not find a pin")))
		{
			ErrorMsg += TEXT("\n[AI HINT: You attempted to connect to a pin that does not exist. Pin names are strictly case-sensitive. Use get_blueprint_info to read the exact pin names for this node type before injecting T3D.]");
		}
		else if (ErrorMsg.Contains(TEXT("are not compatible")) || ErrorMsg.Contains(TEXT("type mismatch")))
		{
			ErrorMsg += TEXT("\n[AI HINT: Pin connection type mismatch. You cannot connect these directly. You may need to inject a conversion node (e.g., Cast, ToString, BreakStruct) between them.]");
		}
		else if (ErrorMsg.Contains(TEXT("FindPropertyByName failed")))
		{
			ErrorMsg += TEXT("\n[AI HINT: The property name does not exist on this object. Remember that internal C++ property names often differ from Editor display names (e.g., 'bHidden' instead of 'Hidden'). Check the class header.]");
		}
		else if (ErrorMsg.Contains(TEXT("while playing in editor")) || ErrorMsg.Contains(TEXT("during PIE")))
		{
			ErrorMsg += TEXT("\n[AI HINT: CRITICAL: You cannot modify assets, compile Blueprints, or recompile C++ while the game is running. Call stop_pie_session immediately and then retry this tool.]");
		}
		else if (ErrorMsg.Contains(TEXT("read-only")) || ErrorMsg.Contains(TEXT("checkout")))
		{
			ErrorMsg += TEXT("\n[AI HINT: The asset is locked by Perforce/Source Control. You must call source_control_checkout on this asset path before attempting to modify it.]");
		}
	}
};

FAgentFrameworkActionResult FAgentFrameworkActionRouter::RouteToolCall(const FAgentFrameworkToolCall& ToolCall)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	struct FScopedRouterTelemetry
	{
		FString ToolName;
		TSharedPtr<FJsonObject> InputParams;
		double StartTime;
		FAgentFrameworkActionResult Result;

		FScopedRouterTelemetry(const FString& InToolName, TSharedPtr<FJsonObject> InParams)
			: ToolName(InToolName), InputParams(InParams), StartTime(FPlatformTime::Seconds()) {}

		~FScopedRouterTelemetry()
		{
			double DurationMicros = (FPlatformTime::Seconds() - StartTime) * 1000000.0;
			if (DurationMicros < 0.0)
			{
				DurationMicros = 0.0;
			}

			FString ContextSummary;
			if (InputParams.IsValid())
			{
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ContextSummary);
				FJsonSerializer::Serialize(InputParams.ToSharedRef(), Writer);
				if (ContextSummary.Len() > 256)
				{
					ContextSummary = ContextSummary.Left(256) + TEXT("...");
				}
			}

			FAgentFrameworkActionRouter::OnToolExecutionRecorded.Broadcast(ToolName, DurationMicros, Result.bSuccess, Result.Errors, ContextSummary);
		}
	} ScopedTelemetry(ToolCall.ToolName, ToolCall.InputParams);

	TSharedPtr<IAgentFrameworkActionExecutor> Executor = FindExecutorForTool(ToolCall.ToolName);
	if (!Executor.IsValid())
	{
		ScopedTelemetry.Result.bSuccess = false;
		ScopedTelemetry.Result.Errors.Add(FString::Printf(TEXT("No executor registered for tool: %s"), *ToolCall.ToolName));
		return ScopedTelemetry.Result;
	}

	// CRITICAL: Validate asset_path before dispatching to prevent UE modal dialog freeze.
	// UE's CreatePackage() / FPackageName functions show FMessageDialog::Open() on invalid
	// paths, which freezes the editor. We catch this early and return a clean error.
	{
		FString ValidationError;
		if (!ValidateAssetPathParam(ToolCall.InputParams, ValidationError))
		{
			ScopedTelemetry.Result.bSuccess = false;
			ScopedTelemetry.Result.Errors.Add(ValidationError);
			UE_LOG(LogAgentFramework, Warning, TEXT("ActionRouter: Blocked tool '%s' — invalid asset_path: %s"),
				*ToolCall.ToolName, *ValidationError);
			return ScopedTelemetry.Result;
		}
	}

	// CRITICAL: Inject the tool name into the params so executors can dispatch.
	TSharedRef<FJsonObject> ParamsWithToolName = MakeShared<FJsonObject>();
	if (ToolCall.InputParams.IsValid())
	{
		// Copy all fields from original params
		for (const auto& Pair : ToolCall.InputParams->Values)
		{
			ParamsWithToolName->SetField(Pair.Key, Pair.Value);
		}
	}
	ParamsWithToolName->SetStringField(TEXT("_tool_name"), ToolCall.ToolName);
	// Also set "tool_name" for backward compatibility with executors that read it
	ParamsWithToolName->SetStringField(TEXT("tool_name"), ToolCall.ToolName);

	// Centrally invoke the executor-specific parameter validation
	TArray<FString> ValidationErrors;
	if (!Executor->ValidateParams(ParamsWithToolName, ValidationErrors))
	{
		ScopedTelemetry.Result.bSuccess = false;
		ScopedTelemetry.Result.Errors = ValidationErrors;
		UE_LOG(LogAgentFramework, Warning, TEXT("ActionRouter: Blocked tool '%s' due to parameter validation failure"), *ToolCall.ToolName);
		return ScopedTelemetry.Result;
	}

#if PLATFORM_WINDOWS
struct FSEHExecutorWrapper
{
	static DWORD Execute(IAgentFrameworkActionExecutor* Executor, const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult* OutResult)
	{
		DWORD ExceptionCode = 0;
		__try
		{
			InvokeExecutor(Executor, Params, OutResult);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			ExceptionCode = GetExceptionCode();
		}
		return ExceptionCode;
	}

private:
	static void InvokeExecutor(IAgentFrameworkActionExecutor* Executor, const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult* OutResult)
	{
		*OutResult = Executor->ExecuteAction(Params);
	}
};
#endif

	uint64 BaselineLogIndex = 0;
	TSharedPtr<FAgentFrameworkLogCapture> LogCapture = FAgentFrameworkLogCapture::Get();
	if (LogCapture.IsValid())
	{
		BaselineLogIndex = LogCapture->GetSnapshotIndex();
	}

#if PLATFORM_WINDOWS
	DWORD SEHCode = FSEHExecutorWrapper::Execute(Executor.Get(), ParamsWithToolName, &ScopedTelemetry.Result);
	if (SEHCode != 0)
	{
		ScopedTelemetry.Result.bSuccess = false;
		ScopedTelemetry.Result.Errors.Add(FString::Printf(TEXT("CRASH GUARDRAIL: Intercepted fatal Access Violation / SEH Exception (0x%08X) during execution of tool '%s'."), SEHCode, *ToolCall.ToolName));
		UE_LOG(LogAgentFramework, Error, TEXT("ActionRouter: Intercepted SEH exception 0x%08X during tool '%s'"), SEHCode, *ToolCall.ToolName);
	}
#else
	ScopedTelemetry.Result = Executor->ExecuteAction(ParamsWithToolName);
#endif

	// Fold engine log output emitted during this tool call back into the response.
	//
	// Agents evaluate a tool call almost entirely from its payload, so a call that
	// returns { bSuccess: true } while the engine logged an error or warning in the
	// background reads as a clean success. Both the text AND the Warnings array are
	// populated: Warnings is the structured channel, but ResultMessage is what the MCP
	// bridge relays verbatim on success, so the text copy is what the agent actually reads.
	if (LogCapture.IsValid() && ToolCall.ToolName != TEXT("read_message_log"))
	{
		FAgentFrameworkLogDelta LogDelta;
		LogCapture->GetLogDeltaEntries(BaselineLogIndex, LogDelta);

		const TArray<FString>& CapturedErrors = LogDelta.Errors;
		const TArray<FString>& CapturedWarnings = LogDelta.Warnings;

		// Say how many entries exist, not how many survived the cap — a truncated list
		// presented as the complete set is the blindness this whole mechanism exists to fix.
		auto DescribeCount = [](int32 Shown, int32 Total)
		{
			return (Total > Shown)
				? FString::Printf(TEXT("%d, showing first %d"), Total, Shown)
				: FString::Printf(TEXT("%d"), Total);
		};

		// Drop anything the executor already reported so the agent does not read it twice.
		auto IsAlreadyReported = [&ScopedTelemetry](const FString& Line)
		{
			// Guard the empty case: FString::Contains(TEXT("")) is always true, so an empty
			// entry in Errors/Warnings would suppress every captured line.
			if (Line.IsEmpty())
			{
				return true;
			}

			auto Overlaps = [&Line](const TArray<FString>& Reported)
			{
				for (const FString& Existing : Reported)
				{
					if (!Existing.IsEmpty() && (Line.Contains(Existing) || Existing.Contains(Line)))
					{
						return true;
					}
				}
				return false;
			};

			return Overlaps(ScopedTelemetry.Result.Errors) || Overlaps(ScopedTelemetry.Result.Warnings);
		};

		FString Diagnostics;

		if (CapturedErrors.Num() > 0)
		{
			Diagnostics += FString::Printf(TEXT("\n\n--- Diagnostics Log (%s Error(s) emitted during execution) ---\n"),
				*DescribeCount(CapturedErrors.Num(), LogDelta.TotalErrors));
			for (const FString& Line : CapturedErrors)
			{
				Diagnostics += FString::Printf(TEXT("  - %s\n"), *Line);
				if (!IsAlreadyReported(Line))
				{
					ScopedTelemetry.Result.Warnings.Add(FString::Printf(TEXT("ENGINE ERROR LOGGED: %s"), *Line));
				}
			}
		}

		if (CapturedWarnings.Num() > 0)
		{
			Diagnostics += FString::Printf(TEXT("\n\n--- Diagnostics Log (%s Warning(s) emitted during execution) ---\n"),
				*DescribeCount(CapturedWarnings.Num(), LogDelta.TotalWarnings));
			for (const FString& Line : CapturedWarnings)
			{
				Diagnostics += FString::Printf(TEXT("  - %s\n"), *Line);
				if (!IsAlreadyReported(Line))
				{
					ScopedTelemetry.Result.Warnings.Add(Line);
				}
			}
		}

		if (!Diagnostics.IsEmpty() && !FAgentFrameworkDiagnostics::IsOpaquePayload(ScopedTelemetry.Result.ResultMessage))
		{
			if (CapturedErrors.Num() > 0 && ScopedTelemetry.Result.bSuccess)
			{
				Diagnostics += TEXT("\n[AI HINT: This tool reported success but the engine logged error(s) above. "
					"Verify the asset/state actually changed as intended before moving on — do not assume success.]");
			}
			ScopedTelemetry.Result.ResultMessage += Diagnostics;
		}
	}

	if (!ScopedTelemetry.Result.bSuccess || ScopedTelemetry.Result.Errors.Num() > 0)
	{
		for (FString& ErrorMsg : ScopedTelemetry.Result.Errors)
		{
			FAgentFrameworkDiagnostics::EnrichErrorString(ErrorMsg);
		}
	}

	return ScopedTelemetry.Result;
}



TSharedPtr<IAgentFrameworkActionExecutor> FAgentFrameworkActionRouter::FindExecutorForTool(const FString& ToolName) const
{
	const TSharedRef<IAgentFrameworkActionExecutor>* Found = ExecutorMap.Find(ToolName);
	return Found ? TSharedPtr<IAgentFrameworkActionExecutor>(*Found) : nullptr;
}

FGuid FAgentFrameworkActionRouter::RouteToolCallAsync(const FAgentFrameworkToolCall& ToolCall, TFunction<void(FAgentFrameworkActionResult)> OnComplete)
{
	FAgentFrameworkAsyncTaskHandle TaskHandle;
	TaskHandle.TaskId = FGuid::NewGuid();
	TaskHandle.ToolCall = ToolCall;
	TaskHandle.OnComplete = MoveTemp(OnComplete);
	TaskHandle.EnqueueTime = FPlatformTime::Seconds();

	{
		FScopeLock Lock(&TaskQueueCS);
		PendingTasks.Add(TaskHandle);
	}

	UE_LOG(LogAgentFramework, Verbose, TEXT("ActionRouter: Enqueued async tool call '%s' (TaskId: %s)"),
		*ToolCall.ToolName, *TaskHandle.TaskId.ToString());

	TWeakPtr<FAgentFrameworkActionRouter> WeakSelf = AsShared();
	AsyncTask(ENamedThreads::GameThread, [WeakSelf]()
	{
		if (TSharedPtr<FAgentFrameworkActionRouter> StrongSelf = WeakSelf.Pin())
		{
			StrongSelf->ProcessTaskQueue();
		}
	});

	return TaskHandle.TaskId;
}

int32 FAgentFrameworkActionRouter::GetPendingTaskCount() const
{
	FScopeLock Lock(&TaskQueueCS);
	return PendingTasks.Num();
}

bool FAgentFrameworkActionRouter::CancelTask(const FGuid& TaskId)
{
	FScopeLock Lock(&TaskQueueCS);
	for (int32 Index = 0; Index < PendingTasks.Num(); ++Index)
	{
		if (PendingTasks[Index].TaskId == TaskId)
		{
			FAgentFrameworkAsyncTaskHandle CancelledTask = PendingTasks[Index];
			PendingTasks.RemoveAt(Index);

			if (CancelledTask.OnComplete)
			{
				FAgentFrameworkActionResult CancelledResult;
				CancelledResult.bSuccess = false;
				CancelledResult.Errors.Add(FString::Printf(TEXT("Tool call '%s' (TaskId: %s) was cancelled before execution"),
					*CancelledTask.ToolCall.ToolName, *TaskId.ToString()));
				CancelledTask.OnComplete(CancelledResult);
			}
			return true;
		}
	}
	return false;
}

void FAgentFrameworkActionRouter::ClearPendingTasks()
{
	TArray<FAgentFrameworkAsyncTaskHandle> TasksToCancel;
	{
		FScopeLock Lock(&TaskQueueCS);
		TasksToCancel = MoveTemp(PendingTasks);
		PendingTasks.Empty();
	}

	for (auto& Task : TasksToCancel)
	{
		if (Task.OnComplete)
		{
			FAgentFrameworkActionResult CancelledResult;
			CancelledResult.bSuccess = false;
			CancelledResult.Errors.Add(TEXT("Task cancelled: ActionRouter is shutting down"));
			Task.OnComplete(CancelledResult);
		}
	}
}

void FAgentFrameworkActionRouter::ProcessTaskQueue()
{
	check(IsInGameThread());

	FAgentFrameworkAsyncTaskHandle CurrentTask;
	bool bHasTask = false;

	{
		FScopeLock Lock(&TaskQueueCS);
		if (PendingTasks.Num() > 0)
		{
			CurrentTask = PendingTasks[0];
			PendingTasks.RemoveAt(0);
			bHasTask = true;
		}
	}

	if (bHasTask)
	{
		UE_LOG(LogAgentFramework, Verbose, TEXT("ActionRouter: Executing async tool call '%s' (TaskId: %s) on Game Thread"),
			*CurrentTask.ToolCall.ToolName, *CurrentTask.TaskId.ToString());

		FAgentFrameworkActionResult Result = RouteToolCall(CurrentTask.ToolCall);

		if (CurrentTask.OnComplete)
		{
			CurrentTask.OnComplete(Result);
		}
	}
}

