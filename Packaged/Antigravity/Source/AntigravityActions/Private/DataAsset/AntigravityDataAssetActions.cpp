// Copyright 2026 Antigravity. All Rights Reserved.

#include "DataAsset/AntigravityDataAssetActions.h"
#include "AntigravityCoreModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataAssetFactory.h"
#include "Engine/DataAsset.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "AntigravityDataAssetActions"

FAntigravityDataAssetActions::FAntigravityDataAssetActions() {}
FAntigravityDataAssetActions::~FAntigravityDataAssetActions() {}

FName FAntigravityDataAssetActions::GetActionName() const { return FName(TEXT("DataAsset")); }
FText FAntigravityDataAssetActions::GetDisplayName() const { return LOCTEXT("DisplayName", "Data Asset Tools"); }
EAntigravityActionCategory FAntigravityDataAssetActions::GetCategory() const { return EAntigravityActionCategory::General; }
EAntigravityRiskLevel FAntigravityDataAssetActions::GetDefaultRiskLevel() const { return EAntigravityRiskLevel::Medium; }
bool FAntigravityDataAssetActions::CanUndo() const { return true; }
bool FAntigravityDataAssetActions::UndoAction() { return GEditor && GEditor->UndoTransaction(); }

TArray<FString> FAntigravityDataAssetActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_data_asset"),
		TEXT("set_data_asset_properties"),
		TEXT("get_data_asset_info")
	};
}

bool FAntigravityDataAssetActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	return true;
}

FAntigravityActionPlan FAntigravityDataAssetActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionPlan Plan;
	FString Action;
	Params->TryGetStringField(TEXT("action"), Action);
	if (Action.IsEmpty()) Params->TryGetStringField(TEXT("tool_name"), Action);

	if (Action == TEXT("create_data_asset"))
	{
		FString AssetPath;
		Params->TryGetStringField(TEXT("asset_path"), AssetPath);
		Plan.Summary = FString::Printf(TEXT("Create Data Asset: %s"), *AssetPath);
	}
	else if (Action == TEXT("set_data_asset_properties"))
	{
		FString AssetPath;
		Params->TryGetStringField(TEXT("asset_path"), AssetPath);
		Plan.Summary = FString::Printf(TEXT("Set properties on Data Asset: %s"), *AssetPath);
	}
	else
	{
		FString AssetPath;
		Params->TryGetStringField(TEXT("asset_path"), AssetPath);
		Plan.Summary = FString::Printf(TEXT("Inspect Data Asset: %s"), *AssetPath);
	}

	Plan.MaxRiskLevel = EAntigravityRiskLevel::Medium;
	FAntigravityAction A;
	A.Description = Plan.Summary;
	A.Category = EAntigravityActionCategory::General;
	A.RiskLevel = EAntigravityRiskLevel::Medium;
	Plan.Actions.Add(A);
	return Plan;
}

FAntigravityActionResult FAntigravityDataAssetActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionResult Result;
	Result.bSuccess = false;

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action) || Action.IsEmpty())
		Params->TryGetStringField(TEXT("tool_name"), Action);

	if (Action == TEXT("create_data_asset"))
		return ExecuteCreateDataAsset(Params, Result);
	else if (Action == TEXT("set_data_asset_properties"))
		return ExecuteSetDataAssetProperties(Params, Result);
	else if (Action == TEXT("get_data_asset_info"))
		return ExecuteGetDataAssetInfo(Params, Result);

	Result.Errors.Add(TEXT("Could not determine DataAsset action."));
	return Result;
}

FAntigravityActionResult FAntigravityDataAssetActions::ExecuteCreateDataAsset(
	const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Result.Errors.Add(TEXT("Missing required field: 'asset_path'"));
		return Result;
	}

	FString ClassName;
	if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
	{
		Result.Errors.Add(TEXT("Missing required field: 'class_name'"));
		return Result;
	}

	// Try finding the class
	UClass* TargetClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
	if (!TargetClass)
	{
		// Try loading class from path (e.g. Blueprint class ending in _C)
		TargetClass = StaticLoadClass(UDataAsset::StaticClass(), nullptr, *ClassName);
	}

	if (!TargetClass)
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
	Factory->DataAssetClass = TargetClass;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, TargetClass, Factory);
	if (!NewAsset)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create Data Asset at '%s'."), *AssetPath));
		return Result;
	}

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully created Data Asset '%s' of class '%s'."), *AssetPath, *TargetClass->GetName());
	return Result;
}

FAntigravityActionResult FAntigravityDataAssetActions::ExecuteSetDataAssetProperties(
	const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Result.Errors.Add(TEXT("Missing required field: 'asset_path'"));
		return Result;
	}

	UDataAsset* DataAsset = LoadObject<UDataAsset>(nullptr, *AssetPath);
	if (!DataAsset)
	{
		Result.Errors.Add(FString::Printf(TEXT("Data Asset not found at '%s'. Create it first with create_data_asset."), *AssetPath));
		return Result;
	}

	const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
	if (!Params->TryGetObjectField(TEXT("properties"), PropertiesObj) || !PropertiesObj)
	{
		Result.Errors.Add(TEXT("Missing required field: 'properties'"));
		return Result;
	}

	DataAsset->Modify();

	for (const auto& Pair : (*PropertiesObj)->Values)
	{
		FString PropName = Pair.Key;
		FProperty* Prop = DataAsset->GetClass()->FindPropertyByName(FName(*PropName));
		if (!Prop)
		{
			Result.Warnings.Add(FString::Printf(TEXT("Property '%s' not found on class '%s'."), *PropName, *DataAsset->GetClass()->GetName()));
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

	DataAsset->MarkPackageDirty();

	Result.bSuccess = true;
	Result.ModifiedAssets.Add(AssetPath);
	Result.ResultMessage = FString::Printf(TEXT("Successfully updated properties on Data Asset '%s'."), *AssetPath);
	return Result;
}

FAntigravityActionResult FAntigravityDataAssetActions::ExecuteGetDataAssetInfo(
	const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		Result.Errors.Add(TEXT("Missing required field: 'asset_path'"));
		return Result;
	}

	UDataAsset* DataAsset = LoadObject<UDataAsset>(nullptr, *AssetPath);
	if (!DataAsset)
	{
		Result.Errors.Add(FString::Printf(TEXT("Data Asset not found at '%s'."), *AssetPath));
		return Result;
	}

	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetStringField(TEXT("asset_path"), AssetPath);
	ResponseObj->SetStringField(TEXT("class_name"), DataAsset->GetClass()->GetName());

	TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
	for (TFieldIterator<FProperty> It(DataAsset->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop)
		{
			// Skip properties belonging to base UObject, UDataAsset, or UPrimaryDataAsset
			UClass* OwnerClass = Prop->GetOwnerClass();
			if (OwnerClass == UObject::StaticClass() || OwnerClass == UDataAsset::StaticClass() || OwnerClass == UPrimaryDataAsset::StaticClass())
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

#undef LOCTEXT_NAMESPACE
