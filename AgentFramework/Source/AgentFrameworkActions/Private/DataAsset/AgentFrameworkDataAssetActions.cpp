// Copyright 2026 AgentFramework. All Rights Reserved.

#include "DataAsset/AgentFrameworkDataAssetActions.h"
#include "AgentFrameworkActionUtils.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataAssetFactory.h"
#include "Engine/DataAsset.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Sound/SoundBase.h"
#endif

#define LOCTEXT_NAMESPACE "AgentFrameworkDataAssetActions"

FAgentFrameworkDataAssetActions::FAgentFrameworkDataAssetActions() {}
FAgentFrameworkDataAssetActions::~FAgentFrameworkDataAssetActions() {}

FName FAgentFrameworkDataAssetActions::GetActionName() const { return FName(TEXT("DataAsset")); }

TArray<FString> FAgentFrameworkDataAssetActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_data_asset"),
		TEXT("set_data_asset_properties"),
		TEXT("get_data_asset_info")
	};
}

bool FAgentFrameworkDataAssetActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAgentFrameworkActionResult FAgentFrameworkDataAssetActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString Action;
	TArray<FString> TempErrors;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action"), Action, TempErrors, false) || Action.IsEmpty())
	{
		UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("tool_name"), Action, TempErrors, false);
	}

	if (Action == TEXT("create_data_asset"))
	{
		Result = ExecuteCreateDataAsset(Params, Result);
	}
	else if (Action == TEXT("set_data_asset_properties"))
	{
		Result = ExecuteSetDataAssetProperties(Params, Result);
	}
	else if (Action == TEXT("get_data_asset_info"))
	{
		Result = ExecuteGetDataAssetInfo(Params, Result);
	}
	else
	{
		Result.Errors.Add(TEXT("Could not determine DataAsset action."));
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkDataAssetActions::ExecuteCreateDataAsset(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString ClassName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("class_name"), ClassName, Result.Errors, true))
	{
		return Result;
	}

	// Try finding the class
	UClass* TargetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
	if (!IsValid(TargetClass))
	{
		// Try loading class from path (e.g. Blueprint class ending in _C)
		TargetClass = StaticLoadClass(UDataAsset::StaticClass(), nullptr, *ClassName);
	}

	if (!IsValid(TargetClass))
	{
		Result.Errors.Add(FString::Printf(TEXT("DataAsset class '%s' not found. If this is a Blueprint class, ensure the path ends with '_C'."), *ClassName));
		return Result;
	}

	if (!TargetClass->IsChildOf(UDataAsset::StaticClass()))
	{
		Result.Errors.Add(FString::Printf(TEXT("Class '%s' does not derive from UDataAsset."), *ClassName));
		return Result;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	if (!IsValid(Factory))
	{
		Result.Errors.Add(TEXT("Failed to create UDataAssetFactory."));
		return Result;
	}
	Factory->DataAssetClass = TargetClass;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, TargetClass, Factory);
	if (!IsValid(NewAsset))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Data Asset at '%s'."), *AssetPath));
		return Result;
	}

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully created Data Asset '%s' of class '%s'."), *AssetPath, *TargetClass->GetName());
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkDataAssetActions::ExecuteSetDataAssetProperties(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UDataAsset* DataAsset = LoadObject<UDataAsset>(nullptr, *AssetPath);
	if (!IsValid(DataAsset))
	{
		Result.Errors.Add(FString::Printf(TEXT("Data Asset not found at '%s'. Create it first with create_data_asset."), *AssetPath));
		return Result;
	}

	const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
	if (!UAgentFrameworkActionUtils::TryGetObjectParam(Params, TEXT("properties"), PropertiesObj, Result.Errors, true))
	{
		return Result;
	}

	DataAsset->Modify();

	UClass* DataAssetClass = DataAsset->GetClass();
	if (IsValid(DataAssetClass))
	{
		for (const auto& Pair : (*PropertiesObj)->Values)
		{
			FString PropName = FString(*Pair.Key);
			FProperty* Prop = DataAssetClass->FindPropertyByName(FName(*PropName));
			if (!Prop)
			{
				Result.Warnings.Add(FString::Printf(TEXT("Property '%s' not found on class '%s'."), *PropName, *DataAssetClass->GetName()));
				continue;
			}

			FString ValueStr;
			if (Pair.Value->TryGetString(ValueStr))
			{
				void* PropAddr = Prop->ContainerPtrToValuePtr<void>(DataAsset);
				Prop->ImportText_Direct(*ValueStr, PropAddr, DataAsset, PPF_None);

				FPropertyChangedEvent ChangedEvent(Prop);
				DataAsset->PostEditChangeProperty(ChangedEvent);
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Property '%s' value is not a string."), *PropName));
			}
		}
	}

	DataAsset->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully updated properties on Data Asset '%s'."), *AssetPath);
	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkDataAssetActions::ExecuteGetDataAssetInfo(
	const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	UDataAsset* DataAsset = LoadObject<UDataAsset>(nullptr, *AssetPath);
	if (!IsValid(DataAsset))
	{
		Result.Errors.Add(FString::Printf(TEXT("Data Asset not found at '%s'."), *AssetPath));
		return Result;
	}

	UClass* DataAssetClass = DataAsset->GetClass();
	if (!IsValid(DataAssetClass))
	{
		Result.Errors.Add(TEXT("Data Asset has an invalid class."));
		return Result;
	}

	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetStringField(TEXT("asset_path"), AssetPath);
	ResponseObj->SetStringField(TEXT("class_name"), DataAssetClass->GetName());

	TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> It(DataAssetClass); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop)
		{
			// Skip properties belonging to base UObject, UDataAsset, or UPrimaryDataAsset
			UClass* OwnerClass = Prop->GetOwnerClass();
			if (IsValid(OwnerClass) && (OwnerClass == UObject::StaticClass() || OwnerClass == UDataAsset::StaticClass() || OwnerClass == UPrimaryDataAsset::StaticClass()))
			{
				continue;
			}

			FString ValueStr;
			void* PropAddr = Prop->ContainerPtrToValuePtr<void>(DataAsset);
			Prop->ExportTextItem_Direct(ValueStr, PropAddr, nullptr, nullptr, PPF_None);
			PropertiesObj->SetStringField(Prop->GetName(), ValueStr);
		}
	}
	ResponseObj->SetObjectField(TEXT("properties"), PropertiesObj);

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);

	Result.bSuccess = true;
	Result.ResultMessage = ResponseString;
	return Result;
}

void FAgentFrameworkDataAssetActions::PlaySuccessSound()
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
