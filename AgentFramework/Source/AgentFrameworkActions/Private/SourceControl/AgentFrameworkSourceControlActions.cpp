// Copyright 2026 AgentFramework. All Rights Reserved.

#include "SourceControl/AgentFrameworkSourceControlActions.h"
#include "AgentFrameworkActionUtils.h"
#include "AgentFrameworkCoreModule.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "ISourceControlRevision.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"

FAgentFrameworkSourceControlActions::FAgentFrameworkSourceControlActions() {}
FAgentFrameworkSourceControlActions::~FAgentFrameworkSourceControlActions() {}
FName FAgentFrameworkSourceControlActions::GetActionName() const { return FName(TEXT("SourceControl")); }

TArray<FString> FAgentFrameworkSourceControlActions::GetSupportedToolNames() const
{
	return {
		TEXT("source_control_checkout"),
		TEXT("source_control_add"),
		TEXT("source_control_revert"),
		TEXT("source_control_status"),
		TEXT("source_control_checkin"),
		TEXT("source_control_sync"),
		TEXT("source_control_history"),
		TEXT("source_control_diff")
	};
}

bool FAgentFrameworkSourceControlActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkSourceControlActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	if (!ISourceControlModule::Get().IsEnabled())
	{
		Result.Errors.Add(TEXT("Source control is not enabled in the editor."));
		return Result;
	}

	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();

	FString Action;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), Action, Result.Errors, false);

	if (Action.IsEmpty())
	{
		FString ToolName;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), ToolName, Result.Errors, false) || ToolName.IsEmpty())
		{
			UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, Result.Errors, false);
		}

		if (ToolName == TEXT("source_control_checkout"))
		{
			Action = TEXT("checkout");
		}
		else if (ToolName == TEXT("source_control_add"))
		{
			Action = TEXT("add");
		}
		else if (ToolName == TEXT("source_control_revert"))
		{
			Action = TEXT("revert");
		}
		else if (ToolName == TEXT("source_control_status"))
		{
			Action = TEXT("status");
		}
		else if (ToolName == TEXT("source_control_checkin") || ToolName == TEXT("source_control_submit"))
		{
			Action = TEXT("checkin");
		}
		else if (ToolName == TEXT("source_control_sync"))
		{
			Action = TEXT("sync");
		}
		else if (ToolName == TEXT("source_control_history"))
		{
			Action = TEXT("history");
		}
		else if (ToolName == TEXT("source_control_diff"))
		{
			Action = TEXT("diff");
		}
	}

	if (Action.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing 'action' field or valid tool name."));
		return Result;
	}

	if (Action != TEXT("status") && !Provider.IsAvailable())
	{
		Result.Errors.Add(TEXT("Source control provider is not available/connected. Please login or configure the connection in the editor."));
		return Result;
	}

	// Extract file paths using standard UAgentFrameworkActionUtils helpers
	TArray<FString> FilePaths;
	if (!UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("file_paths"), FilePaths, Result.Errors, false) || FilePaths.Num() == 0)
	{
		if (!UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("files"), FilePaths, Result.Errors, false) || FilePaths.Num() == 0)
		{
			FString SinglePath;
			if (UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("file"), SinglePath, Result.Errors, false) && !SinglePath.IsEmpty())
			{
				FilePaths.Add(SinglePath);
			}
		}
	}

	if (Action == TEXT("checkout"))
	{
		if (FilePaths.Num() == 0)
		{
			Result.Errors.Add(TEXT("No files specified for checkout."));
			return Result;
		}

		bool bSuccess = USourceControlHelpers::CheckOutFiles(FilePaths);
		Result.bSuccess = bSuccess;
		Result.ResultMessage = bSuccess
			? FString::Printf(TEXT("Checked out %d file(s)."), FilePaths.Num())
			: TEXT("Failed to check out files.");
	}
	else if (Action == TEXT("add"))
	{
		if (FilePaths.Num() == 0)
		{
			Result.Errors.Add(TEXT("No files specified for add."));
			return Result;
		}

		bool bSuccess = USourceControlHelpers::MarkFilesForAdd(FilePaths);
		Result.bSuccess = bSuccess;
		Result.ResultMessage = bSuccess
			? FString::Printf(TEXT("Marked %d file(s) for add."), FilePaths.Num())
			: TEXT("Failed to mark files for add.");
	}
	else if (Action == TEXT("revert"))
	{
		if (FilePaths.Num() == 0)
		{
			Result.Errors.Add(TEXT("No files specified for revert."));
			return Result;
		}

		TSharedRef<FRevert, ESPMode::ThreadSafe> RevertOp = ISourceControlOperation::Create<FRevert>();
		ECommandResult::Type OpResult = Provider.Execute(RevertOp, FilePaths);
		Result.bSuccess = (OpResult == ECommandResult::Succeeded);
		Result.ResultMessage = Result.bSuccess
			? FString::Printf(TEXT("Reverted %d file(s)."), FilePaths.Num())
			: TEXT("Revert operation failed.");
	}
	else if (Action == TEXT("status"))
	{
		FString StatusReport = TEXT("Source Control Status:\n");
		StatusReport += FString::Printf(TEXT("Provider: %s\n"), *Provider.GetName().ToString());
		StatusReport += FString::Printf(TEXT("Connected: %s\n"), Provider.IsAvailable() ? TEXT("Yes") : TEXT("No"));

		if (FilePaths.Num() > 0)
		{
			for (const FString& Path : FilePaths)
			{
				FSourceControlStatePtr State = Provider.GetState(Path, EStateCacheUsage::ForceUpdate);
				if (State.IsValid())
				{
					StatusReport += FString::Printf(TEXT("  %s: %s (CheckedOut: %s, Modified: %s, Added: %s)\n"),
						*Path,
						*State->GetDisplayName().ToString(),
						State->IsCheckedOut() ? TEXT("Yes") : TEXT("No"),
						State->IsModified() ? TEXT("Yes") : TEXT("No"),
						State->IsAdded() ? TEXT("Yes") : TEXT("No"));
				}
				else
				{
					StatusReport += FString::Printf(TEXT("  %s: State Unknown\n"), *Path);
				}
			}
		}

		Result.bSuccess = true;
		Result.ResultMessage = StatusReport;
	}
	else if (Action == TEXT("checkin") || Action == TEXT("submit"))
	{
		if (FilePaths.Num() == 0)
		{
			Result.Errors.Add(TEXT("No files specified for checkin."));
			return Result;
		}

		FString Description;
		if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("description"), Description, Result.Errors, false) || Description.IsEmpty())
		{
			UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("message"), Description, Result.Errors, false);
		}
		if (Description.IsEmpty())
		{
			Description = TEXT("Submitted via AgentFramework");
		}

		TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOp = ISourceControlOperation::Create<FCheckIn>();
		CheckInOp->SetDescription(FText::FromString(Description));
		ECommandResult::Type OpResult = Provider.Execute(CheckInOp, FilePaths);
		Result.bSuccess = (OpResult == ECommandResult::Succeeded);
		Result.ResultMessage = Result.bSuccess
			? FString::Printf(TEXT("Checked in %d file(s)."), FilePaths.Num())
			: TEXT("Checkin operation failed.");
	}
	else if (Action == TEXT("sync"))
	{
		if (FilePaths.Num() == 0)
		{
			Result.Errors.Add(TEXT("No files specified for sync."));
			return Result;
		}

		TSharedRef<FSync, ESPMode::ThreadSafe> SyncOp = ISourceControlOperation::Create<FSync>();
		ECommandResult::Type OpResult = Provider.Execute(SyncOp, FilePaths);
		Result.bSuccess = (OpResult == ECommandResult::Succeeded);
		Result.ResultMessage = Result.bSuccess
			? FString::Printf(TEXT("Synced %d file(s)."), FilePaths.Num())
			: TEXT("Sync operation failed.");
	}
	else if (Action == TEXT("history"))
	{
		if (FilePaths.Num() == 0)
		{
			Result.Errors.Add(TEXT("No files specified for history."));
			return Result;
		}

		TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOp = ISourceControlOperation::Create<FUpdateStatus>();
		UpdateStatusOp->SetUpdateHistory(true);
		Provider.Execute(UpdateStatusOp, FilePaths);

		FString HistoryReport = TEXT("Source Control History:\n");
		for (const FString& Path : FilePaths)
		{
			FSourceControlStatePtr State = Provider.GetState(Path, EStateCacheUsage::Use);
			if (State.IsValid())
			{
				int32 HistorySize = State->GetHistorySize();
				HistoryReport += FString::Printf(TEXT("File: %s (History items: %d)\n"), *Path, HistorySize);
				for (int32 i = 0; i < HistorySize && i < 10; ++i)
				{
					auto Revision = State->GetHistoryItem(i);
					if (Revision.IsValid())
					{
						HistoryReport += FString::Printf(TEXT("  Rev %s by %s on %s: %s\n"),
							*Revision->GetRevision(),
							*Revision->GetUserName(),
							*Revision->GetDate().ToString(),
							*Revision->GetDescription());
					}
				}
			}
			else
			{
				HistoryReport += FString::Printf(TEXT("File: %s - State unavailable\n"), *Path);
			}
		}

		Result.bSuccess = true;
		Result.ResultMessage = HistoryReport;
	}
	else if (Action == TEXT("diff"))
	{
		if (FilePaths.Num() == 0)
		{
			Result.Errors.Add(TEXT("No files specified for diff status."));
			return Result;
		}

		FString DiffReport = TEXT("Source Control Diff Status:\n");
		for (const FString& Path : FilePaths)
		{
			FSourceControlStatePtr State = Provider.GetState(Path, EStateCacheUsage::ForceUpdate);
			if (State.IsValid())
			{
				DiffReport += FString::Printf(TEXT("File: %s\n"), *Path);
				DiffReport += FString::Printf(TEXT("  IsSourceControlled: %s\n"), State->IsSourceControlled() ? TEXT("Yes") : TEXT("No"));
				DiffReport += FString::Printf(TEXT("  IsCheckedOut: %s\n"), State->IsCheckedOut() ? TEXT("Yes") : TEXT("No"));
				DiffReport += FString::Printf(TEXT("  IsCheckedOutOther: %s\n"), State->IsCheckedOutOther() ? TEXT("Yes") : TEXT("No"));
				DiffReport += FString::Printf(TEXT("  IsModified: %s\n"), State->IsModified() ? TEXT("Yes") : TEXT("No"));
				DiffReport += FString::Printf(TEXT("  IsAdded: %s\n"), State->IsAdded() ? TEXT("Yes") : TEXT("No"));
				DiffReport += FString::Printf(TEXT("  IsDeleted: %s\n"), State->IsDeleted() ? TEXT("Yes") : TEXT("No"));
				DiffReport += FString::Printf(TEXT("  IsCurrent: %s\n"), State->IsCurrent() ? TEXT("Yes") : TEXT("No"));
				DiffReport += FString::Printf(TEXT("  CanCheckout: %s\n"), State->CanCheckout() ? TEXT("Yes") : TEXT("No"));
				DiffReport += FString::Printf(TEXT("  CanRevert: %s\n"), State->CanRevert() ? TEXT("Yes") : TEXT("No"));

				auto CurrentRev = State->GetCurrentRevision();
				if (CurrentRev.IsValid())
				{
					DiffReport += FString::Printf(TEXT("  CurrentRevision: %s\n"), *CurrentRev->GetRevision());
				}
			}
			else
			{
				DiffReport += FString::Printf(TEXT("File: %s - State unavailable\n"), *Path);
			}
		}

		Result.bSuccess = true;
		Result.ResultMessage = DiffReport;
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown action: %s"), *Action));
		return Result;
	}

	return Result;
}
