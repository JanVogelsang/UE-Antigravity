#include "Diagnostics/AgentFrameworkDiagnosticsActions.h"
#include "AgentFrameworkActionUtils.h"
#include "AgentFrameworkLogCapture.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Sound/SoundBase.h"
#endif

#define LOCTEXT_NAMESPACE "AgentFrameworkDiagnosticsActions"

// ============================================================================
// Lifecycle
// ============================================================================

FAgentFrameworkDiagnosticsActions::FAgentFrameworkDiagnosticsActions()
{
	// Initialize log capture singleton
	FAgentFrameworkLogCapture::Get();
}

FAgentFrameworkDiagnosticsActions::~FAgentFrameworkDiagnosticsActions()
{
}

// ============================================================================
// IAgentFrameworkActionExecutor Interface
// ============================================================================

FName FAgentFrameworkDiagnosticsActions::GetActionName() const { return FName(TEXT("Diagnostics")); }

TArray<FString> FAgentFrameworkDiagnosticsActions::GetSupportedToolNames() const
{
	return { TEXT("read_message_log"), TEXT("shutdown_editor"), TEXT("find_unreferenced_assets"), TEXT("inspect_uobject_properties") };
}

bool FAgentFrameworkDiagnosticsActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true; // All params optional
}

FAgentFrameworkActionResult FAgentFrameworkDiagnosticsActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	TArray<FString> TempErrors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, TempErrors, false) || ToolName.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), ToolName, TempErrors, false);
	}

	if (ToolName == TEXT("read_message_log"))
	{
		Result = ExecuteReadMessageLog(Params, Result);
	}
	else if (ToolName == TEXT("shutdown_editor"))
	{
		Result = ExecuteShutdownEditor(Params, Result);
	}
	else if (ToolName == TEXT("find_unreferenced_assets"))
	{
		Result = ExecuteFindUnreferencedAssets(Params, Result);
	}
	else if (ToolName == TEXT("inspect_uobject_properties"))
	{
		Result = ExecuteInspectUObjectProperties(Params, Result);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown tool name: %s"), *ToolName));
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	return Result;
}

// ============================================================================
// read_message_log
// ============================================================================

FAgentFrameworkActionResult FAgentFrameworkDiagnosticsActions::ExecuteReadMessageLog(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	TSharedPtr<FAgentFrameworkLogCapture> LogCapture = FAgentFrameworkLogCapture::Get();
	if (!LogCapture.IsValid())
	{
		Result.Errors.Add(TEXT("Log capture device is not initialized."));
		return Result;
	}

	// Parse optional filters
	int32 MaxLines = 100;
	UAgentFrameworkActionUtils::TryGetIntParam(Params, TEXT("max_lines"), MaxLines, Result.Errors, false);
	MaxLines = FMath::Clamp(MaxLines, 1, 500);

	FString CategoryFilter;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("category_filter"), CategoryFilter, Result.Errors, false);

	FString SeverityFilter;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("severity_filter"), SeverityFilter, Result.Errors, false);

	bool bErrorsOnly = false;
	if (SeverityFilter.Equals(TEXT("Error"), ESearchCase::IgnoreCase))
	{
		bErrorsOnly = true;
	}

	bool bWarningsAndErrors = false;
	if (SeverityFilter.Equals(TEXT("Warning"), ESearchCase::IgnoreCase))
	{
		bWarningsAndErrors = true;
	}

	// Filter and format entries
	FString Output = TEXT("=== Output Log ===\n");
	int32 OutputCount = 0;
	int32 TotalErrors = 0;
	int32 TotalWarnings = 0;

	// Read from end (most recent first)
	TArray<FAgentFrameworkLogEntry> Entries;
	LogCapture->GetEntries(Entries);
	int32 StartIdx = FMath::Max(0, Entries.Num() - MaxLines * 2); // Over-read to account for filtering

	for (int32 i = Entries.Num() - 1; i >= StartIdx && OutputCount < MaxLines; --i)
	{
		const auto& Entry = Entries[i];

		// Count stats
		if (Entry.Verbosity == ELogVerbosity::Error || Entry.Verbosity == ELogVerbosity::Fatal)
		{
			TotalErrors++;
		}
		if (Entry.Verbosity == ELogVerbosity::Warning)
		{
			TotalWarnings++;
		}

		// Apply category filter
		if (!CategoryFilter.IsEmpty() && !Entry.Category.Contains(CategoryFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		// Apply severity filter
		if (bErrorsOnly && Entry.Verbosity != ELogVerbosity::Error && Entry.Verbosity != ELogVerbosity::Fatal)
		{
			continue;
		}
		if (bWarningsAndErrors && Entry.Verbosity != ELogVerbosity::Error
			&& Entry.Verbosity != ELogVerbosity::Fatal && Entry.Verbosity != ELogVerbosity::Warning)
		{
			continue;
		}

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
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("clear_after_read"), bClearAfterRead, Result.Errors, false);
	if (bClearAfterRead)
	{
		LogCapture->ClearEntries();
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

FAgentFrameworkActionResult FAgentFrameworkDiagnosticsActions::ExecuteFindUnreferencedAssets(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString FolderPath;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("folder_path"), FolderPath, Result.Errors, false);
	if (FolderPath.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("directory_path"), FolderPath, Result.Errors, false);
	if (FolderPath.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("FolderPath"), FolderPath, Result.Errors, false);
	if (FolderPath.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("target_folder"), FolderPath, Result.Errors, false);

	if (FolderPath.IsEmpty())
	{
		Result.Errors.Add(TEXT("Parameter 'folder_path' is required."));
		return Result;
	}

	if (!FolderPath.StartsWith(TEXT("/Game")))
	{
		if (FolderPath.StartsWith(TEXT("Content/"))) FolderPath = TEXT("/Game/") + FolderPath.RightChop(8);
		else if (FolderPath.StartsWith(TEXT("Content"))) FolderPath = TEXT("/Game");
		else if (!FolderPath.StartsWith(TEXT("/"))) FolderPath = TEXT("/Game/") + FolderPath;
	}
	FPaths::NormalizeDirectoryName(FolderPath);

	bool bIncludeSoftReferences = true;
	if (Params->HasField(TEXT("include_soft_references")))
	{
		UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("include_soft_references"), bIncludeSoftReferences, Result.Errors, false);
	}
	else if (Params->HasField(TEXT("IncludeSoftReferences")))
	{
		UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("IncludeSoftReferences"), bIncludeSoftReferences, Result.Errors, false);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(FName(*FolderPath), Assets, true, false);

	TArray<TSharedPtr<FJsonValue>> UnreferencedAssetsJson;
	TArray<FString> UnreferencedAssetsList;

	FAssetRegistryDependencyOptions DependencyOptions;
	DependencyOptions.bIncludeHardPackageReferences = true;
	DependencyOptions.bIncludeSoftPackageReferences = bIncludeSoftReferences;
	DependencyOptions.bIncludeSearchableNames = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 8
	DependencyOptions.bIncludeManagementReferences = false;
#endif

	for (const FAssetData& Asset : Assets)
	{
		FName PackageName = Asset.PackageName;
		TArray<FName> Referencers;
		AssetRegistry.K2_GetReferencers(PackageName, DependencyOptions, Referencers);

		int32 ExternalReferencersCount = 0;
		for (const FName& Referencer : Referencers)
		{
			if (Referencer != PackageName)
			{
				ExternalReferencersCount++;
			}
		}

		if (ExternalReferencersCount == 0)
		{
			FString AssetPath = Asset.GetObjectPathString();
			UnreferencedAssetsList.Add(AssetPath);
			UnreferencedAssetsJson.Add(MakeShared<FJsonValueString>(AssetPath));
		}
	}

	FString SummaryMsg = FString::Printf(TEXT("Found %d unreferenced assets in %s (include_soft_references: %s)."),
		UnreferencedAssetsList.Num(), *FolderPath, bIncludeSoftReferences ? TEXT("true") : TEXT("false"));

	TSharedPtr<FJsonObject> OutputObj = MakeShared<FJsonObject>();
	OutputObj->SetBoolField(TEXT("bSuccess"), true);
	OutputObj->SetStringField(TEXT("folder_path"), FolderPath);
	OutputObj->SetBoolField(TEXT("include_soft_references"), bIncludeSoftReferences);
	OutputObj->SetArrayField(TEXT("unreferenced_assets"), UnreferencedAssetsJson);
	OutputObj->SetNumberField(TEXT("count"), UnreferencedAssetsList.Num());
	OutputObj->SetStringField(TEXT("ResultMessage"), SummaryMsg);

	FString ResponseJsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseJsonString);
	FJsonSerializer::Serialize(OutputObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseJsonString;
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkDiagnosticsActions::ExecuteInspectUObjectProperties(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString ObjectPath;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("object_path"), ObjectPath, Result.Errors, false);
	if (ObjectPath.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("ObjectPath"), ObjectPath, Result.Errors, false);
	if (ObjectPath.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("target_object"), ObjectPath, Result.Errors, false);

	if (ObjectPath.IsEmpty())
	{
		Result.Errors.Add(TEXT("Parameter 'object_path' is required."));
		return Result;
	}

	bool bIncludeInherited = true;
	if (Params->HasField(TEXT("include_inherited")))
	{
		UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("include_inherited"), bIncludeInherited, Result.Errors, false);
	}
	else if (Params->HasField(TEXT("IncludeInherited")))
	{
		UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("IncludeInherited"), bIncludeInherited, Result.Errors, false);
	}

	UObject* TargetObject = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	if (!TargetObject && !ObjectPath.Contains(TEXT(".")))
	{
		FString AssetName = FPaths::GetBaseFilename(ObjectPath);
		FString FullPath = ObjectPath + TEXT(".") + AssetName;
		TargetObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FullPath);
	}

	if (!TargetObject)
	{
		// Generic CDO Fallback: Search for class name to inspect Class Default Object
		FString ClassName = ObjectPath;
		if (ClassName.StartsWith(TEXT("U")) || ClassName.StartsWith(TEXT("A")))
		{
			ClassName = ClassName.RightChop(1);
		}

		UClass* FoundClass = FindFirstObject<UClass>(*ObjectPath, EFindFirstObjectOptions::None);
		if (!FoundClass)
		{
			FoundClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);
		}

		if (FoundClass)
		{
			TargetObject = FoundClass->GetDefaultObject();
		}
	}

	if (!TargetObject)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load object or resolve Class Default Object at path: %s"), *ObjectPath));
		return Result;
	}

	TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
	UClass* TargetClass = TargetObject->GetClass();

	EFieldIteratorFlags::SuperClassFlags SuperFlags = bIncludeInherited ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;
	for (TFieldIterator<FProperty> It(TargetClass, SuperFlags); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop)
		{
			FString ValueStr;
			const void* PropAddr = Prop->ContainerPtrToValuePtr<void>(TargetObject);
			Prop->ExportTextItem_Direct(ValueStr, PropAddr, nullptr, TargetObject, PPF_None);
			PropertiesObj->SetStringField(Prop->GetName(), ValueStr);
		}
	}

	// Generic Asset Metrics Extraction for Static Mesh
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(TargetObject))
	{
		if (FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData())
		{
			if (RenderData->LODResources.Num() > 0)
			{
				const FStaticMeshLODResources& LOD0 = RenderData->LODResources[0];
				PropertiesObj->SetNumberField(TEXT("LOD0_VertexCount"), LOD0.GetNumVertices());
				PropertiesObj->SetNumberField(TEXT("LOD0_TriangleCount"), LOD0.GetNumTriangles());
			}
		}
	}

	FString SummaryMsg = FString::Printf(TEXT("Successfully inspected properties of %s [%s]."),
		*ObjectPath, *TargetClass->GetName());

	TSharedPtr<FJsonObject> OutputObj = MakeShared<FJsonObject>();
	OutputObj->SetBoolField(TEXT("bSuccess"), true);
	OutputObj->SetStringField(TEXT("object_path"), ObjectPath);
	OutputObj->SetStringField(TEXT("object_class"), TargetClass->GetName());
	OutputObj->SetObjectField(TEXT("properties"), PropertiesObj);
	OutputObj->SetStringField(TEXT("ResultMessage"), SummaryMsg);

	FString ResponseJsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseJsonString);
	FJsonSerializer::Serialize(OutputObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseJsonString;
	return Result;
}

void FAgentFrameworkDiagnosticsActions::PlaySuccessSound()
{
#if WITH_EDITOR
	if (IsValid(GEditor))
	{
		USoundBase* SuccessSound = LoadObject<USoundBase>(nullptr, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
		if (IsValid(SuccessSound))
		{
			GEditor->PlayEditorSound(SuccessSound);
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE
