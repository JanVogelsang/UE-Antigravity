// Copyright 2026 Antigravity. All Rights Reserved.

#include "AntigravityTaskHistory.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FAntigravityTaskHistory::FAntigravityTaskHistory()
{
}

FAntigravityTaskHistory::~FAntigravityTaskHistory()
{
}

bool FAntigravityTaskHistory::Initialize()
{
	LoadFromDisk();
	bIsInitialized = true;
	return true;
}

FString FAntigravityTaskHistory::GetHistoryFilePath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("Antigravity"),
		TEXT("TaskHistory"),
		TEXT("task_history.json")
	);
}

void FAntigravityTaskHistory::RecordTask(const FAntigravityTaskHistoryItem& Item)
{
	// Check if item already exists (by TabId)
	for (int32 i = 0; i < HistoryItems.Num(); i++)
	{
		if (HistoryItems[i].TabId == Item.TabId)
		{
			HistoryItems[i] = Item;
			SaveToDisk();
			return;
		}
	}

	// New item â€” prepend (most recent first)
	HistoryItems.Insert(Item, 0);
	SaveToDisk();
}

void FAntigravityTaskHistory::UpdateTask(const FString& TabId, const FAntigravityTaskHistoryItem& UpdatedItem)
{
	for (FAntigravityTaskHistoryItem& Item : HistoryItems)
	{
		if (Item.TabId == TabId)
		{
			Item = UpdatedItem;
			SaveToDisk();
			return;
		}
	}
}

void FAntigravityTaskHistory::RemoveTask(const FString& TabId)
{
	HistoryItems.RemoveAll([&](const FAntigravityTaskHistoryItem& Item) {
		return Item.TabId == TabId;
	});
	SaveToDisk();
}

void FAntigravityTaskHistory::RenameTask(const FString& TabId, const FString& NewTitle)
{
	for (FAntigravityTaskHistoryItem& Item : HistoryItems)
	{
		if (Item.TabId == TabId)
		{
			Item.Title = NewTitle;
			SaveToDisk();
			return;
		}
	}
}

TArray<FAntigravityTaskHistoryItem> FAntigravityTaskHistory::GetHistory() const
{
	return HistoryItems;
}

const FAntigravityTaskHistoryItem* FAntigravityTaskHistory::GetTask(const FString& TabId) const
{
	for (const FAntigravityTaskHistoryItem& Item : HistoryItems)
	{
		if (Item.TabId == TabId)
		{
			return &Item;
		}
	}
	return nullptr;
}

void FAntigravityTaskHistory::ClearHistory()
{
	HistoryItems.Empty();
	SaveToDisk();
}

FString FAntigravityTaskHistory::ExportAsText() const
{
	FString Output;
	Output += FString::Printf(TEXT("Antigravity Task History (%d sessions)\n"), HistoryItems.Num());
	Output += TEXT("==============================================\n\n");

	for (const FAntigravityTaskHistoryItem& Item : HistoryItems)
	{
		Output += FString::Printf(TEXT("Task: %s\n"), *Item.Title);
		Output += FString::Printf(TEXT("  Created: %s\n"), *Item.CreatedAt.ToString());
		Output += FString::Printf(TEXT("  Messages: %d\n"), Item.MessageCount);
		Output += FString::Printf(TEXT("  Cost: $%.4f\n"), Item.TotalCostUSD);
		Output += FString::Printf(TEXT("  Tokens: %d in / %d out\n"),
			Item.TotalTokenUsage.InputTokens, Item.TotalTokenUsage.OutputTokens);
		if (!Item.FirstUserMessage.IsEmpty())
		{
			Output += FString::Printf(TEXT("  First message: %s\n"), *Item.FirstUserMessage.Left(100));
		}
		Output += TEXT("\n");
	}

	return Output;
}

void FAntigravityTaskHistory::LoadFromDisk()
{
	HistoryItems.Empty();

	const FString FilePath = GetHistoryFilePath();
	if (!FPaths::FileExists(FilePath)) return;

	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *FilePath)) return;

	TArray<TSharedPtr<FJsonValue>> ItemsArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, ItemsArray)) return;

	for (const TSharedPtr<FJsonValue>& Val : ItemsArray)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!Val->TryGetObject(ObjPtr)) continue;
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FAntigravityTaskHistoryItem Item;
		Obj->TryGetStringField(TEXT("tab_id"), Item.TabId);
		Obj->TryGetStringField(TEXT("title"), Item.Title);
		Obj->TryGetNumberField(TEXT("cost_usd"), Item.TotalCostUSD);
		Obj->TryGetNumberField(TEXT("message_count"), Item.MessageCount);
		Obj->TryGetStringField(TEXT("first_message"), Item.FirstUserMessage);
		Obj->TryGetStringField(TEXT("conversation_file"), Item.ConversationFilePath);
		Obj->TryGetStringField(TEXT("model_id"), Item.ModelId);

		// Status
		FString StatusStr;
		if (Obj->TryGetStringField(TEXT("status"), StatusStr))
		{
			if (StatusStr == TEXT("completed"))       Item.Status = EAntigravityTaskStatus::Completed;
			else if (StatusStr == TEXT("interrupted")) Item.Status = EAntigravityTaskStatus::Interrupted;
			else if (StatusStr == TEXT("errored"))     Item.Status = EAntigravityTaskStatus::Errored;
			else                                      Item.Status = EAntigravityTaskStatus::Active;
		}

		FString CreatedStr;
		if (Obj->TryGetStringField(TEXT("created_at"), CreatedStr))
		{
			FDateTime::ParseIso8601(*CreatedStr, Item.CreatedAt);
		}

		FString LastActiveStr;
		if (Obj->TryGetStringField(TEXT("last_active"), LastActiveStr))
		{
			FDateTime::ParseIso8601(*LastActiveStr, Item.LastActiveAt);
		}

		const TSharedPtr<FJsonObject>* UsageObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("token_usage"), UsageObj))
		{
			(*UsageObj)->TryGetNumberField(TEXT("input"), Item.TotalTokenUsage.InputTokens);
			(*UsageObj)->TryGetNumberField(TEXT("output"), Item.TotalTokenUsage.OutputTokens);
		}

		if (!Item.TabId.IsEmpty())
		{
			HistoryItems.Add(Item);
		}
	}
}

void FAntigravityTaskHistory::SaveToDisk() const
{
	const FString FilePath = GetHistoryFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);

	TArray<TSharedPtr<FJsonValue>> ItemsArray;
	for (const FAntigravityTaskHistoryItem& Item : HistoryItems)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("tab_id"), Item.TabId);
		Obj->SetStringField(TEXT("title"), Item.Title);
		Obj->SetNumberField(TEXT("cost_usd"), Item.TotalCostUSD);
		Obj->SetNumberField(TEXT("message_count"), Item.MessageCount);
		Obj->SetStringField(TEXT("first_message"), Item.FirstUserMessage.Left(200));
		Obj->SetStringField(TEXT("conversation_file"), Item.ConversationFilePath);
		Obj->SetStringField(TEXT("created_at"), Item.CreatedAt.ToIso8601());
		Obj->SetStringField(TEXT("last_active"), Item.LastActiveAt.ToIso8601());
		Obj->SetStringField(TEXT("model_id"), Item.ModelId);

		// Status
		FString StatusStr;
		switch (Item.Status)
		{
		case EAntigravityTaskStatus::Completed:   StatusStr = TEXT("completed"); break;
		case EAntigravityTaskStatus::Interrupted:  StatusStr = TEXT("interrupted"); break;
		case EAntigravityTaskStatus::Errored:      StatusStr = TEXT("errored"); break;
		default:                                 StatusStr = TEXT("active"); break;
		}
		Obj->SetStringField(TEXT("status"), StatusStr);

		TSharedPtr<FJsonObject> UsageObj = MakeShared<FJsonObject>();
		UsageObj->SetNumberField(TEXT("input"), Item.TotalTokenUsage.InputTokens);
		UsageObj->SetNumberField(TEXT("output"), Item.TotalTokenUsage.OutputTokens);
		Obj->SetObjectField(TEXT("token_usage"), UsageObj);

		ItemsArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	FString JsonOutput;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonOutput);
	FJsonSerializer::Serialize(ItemsArray, Writer);

	FFileHelper::SaveStringToFile(JsonOutput, *FilePath);
}
