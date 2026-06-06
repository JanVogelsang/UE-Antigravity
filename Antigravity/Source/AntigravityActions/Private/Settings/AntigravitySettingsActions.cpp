// Copyright 2026 Antigravity. All Rights Reserved.

#include "Settings/AntigravitySettingsActions.h"
#include "AntigravityCoreModule.h"
#include "AntigravitySettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/UnrealType.h"
#include "ISettingsModule.h"
#include "ScopedTransaction.h"

FAntigravitySettingsActions::FAntigravitySettingsActions() {}
FAntigravitySettingsActions::~FAntigravitySettingsActions() {}
FName FAntigravitySettingsActions::GetActionName() const { return FName(TEXT("Settings")); }
FText FAntigravitySettingsActions::GetDisplayName() const { return FText::FromString(TEXT("Settings Actions")); }
EAntigravityActionCategory FAntigravitySettingsActions::GetCategory() const { return EAntigravityActionCategory::Settings; }
EAntigravityRiskLevel FAntigravitySettingsActions::GetDefaultRiskLevel() const { return EAntigravityRiskLevel::High; }
bool FAntigravitySettingsActions::CanUndo() const { return false; }
bool FAntigravitySettingsActions::UndoAction() { return false; }

TArray<FString> FAntigravitySettingsActions::GetSupportedToolNames() const
{
	// These must match the tool names in settings_tools.json exactly.
	return {
		TEXT("read_config_value"),
		TEXT("write_config_value"),
		TEXT("macro_ensure_project_prerequisites")
	};
}

bool FAntigravitySettingsActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const { return true; }

FAntigravityActionPlan FAntigravitySettingsActions::PreviewAction(const TSharedRef<FJsonObject>& Params)
{
	FAntigravityActionPlan Plan;
	Plan.Summary = TEXT("Settings modification (HIGH RISK)");
	Plan.MaxRiskLevel = EAntigravityRiskLevel::High;
	FAntigravityAction Action;
	Action.Description = Plan.Summary;
	Action.Category = EAntigravityActionCategory::Settings;
	Action.RiskLevel = EAntigravityRiskLevel::High;
	Plan.Actions.Add(Action);
	return Plan;
}

FAntigravityActionResult FAntigravitySettingsActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FScopedTransaction Transaction(FText::FromString(TEXT("Antigravity Settings Action")));

	FAntigravityActionResult Result;
	Result.bSuccess = false;

	// Dispatch by tool name
	FString ToolName;
	if (Params->TryGetStringField(TEXT("_tool_name"), ToolName) || Params->TryGetStringField(TEXT("tool_name"), ToolName))
	{
		if (ToolName == TEXT("macro_ensure_project_prerequisites"))
		{
			return ExecuteMacroEnsureProjectPrerequisites(Params, Result);
		}
	}

	FString ConfigFile, Section, Key, Value;
	Params->TryGetStringField(TEXT("config_file"), ConfigFile);
	Params->TryGetStringField(TEXT("section"), Section);
	Params->TryGetStringField(TEXT("key"), Key);
	
	// Robust parsing for "value" (handle string, number, or boolean)
	bool bIsWrite = Params->HasField(TEXT("value"));
	if (bIsWrite)
	{
		TSharedPtr<FJsonValue> ValueField = Params->TryGetField(TEXT("value"));
		if (ValueField.IsValid() && !ValueField->IsNull())
		{
			if (ValueField->Type == EJson::String)
			{
				Value = ValueField->AsString();
			}
			else if (ValueField->Type == EJson::Number)
			{
				double NumValue = ValueField->AsNumber();
				if (FMath::TruncToDouble(NumValue) == NumValue)
				{
					Value = FString::Printf(TEXT("%lld"), static_cast<int64>(NumValue));
				}
				else
				{
					Value = FString::SanitizeFloat(NumValue);
				}
			}
			else if (ValueField->Type == EJson::Boolean)
			{
				Value = ValueField->AsBool() ? TEXT("True") : TEXT("False");
			}
		}
	}

	if (Section.IsEmpty() || Key.IsEmpty())
	{
		Result.Errors.Add(TEXT("Missing required fields: 'section' and 'key' are required."));
		return Result;
	}

	if (!bIsWrite)
	{
		// READ CONFIG VALUE
		// Determine which ini to read from
		FString TargetIni = GEngineIni;
		if (ConfigFile.Contains(TEXT("Game"))) TargetIni = GGameIni;
		else if (ConfigFile.Contains(TEXT("Input"))) TargetIni = GInputIni;
		else if (ConfigFile.Contains(TEXT("Editor"))) TargetIni = GEditorIni;

		FString ReadValue;
		if (GConfig->GetString(*Section, *Key, ReadValue, TargetIni))
		{
			Result.bSuccess = true;
			Result.ResultMessage = FString::Printf(TEXT("[%s] %s = %s (from %s)"), *Section, *Key, *ReadValue, *ConfigFile);
		}
		else
		{
			// Return success=true with a "not found" message instead of a hard error.
			// "Not found" is valid diagnostic information, not a tool execution failure.
			// The AI uses this to determine that a key/section doesn't exist and adjusts its approach.
			Result.bSuccess = true;
			Result.ResultMessage = FString::Printf(TEXT("Setting not found: [%s] %s in %s. The key does not exist in this config file."), *Section, *Key, *ConfigFile);
		}
	}
	else
	{
		// WRITE CONFIG VALUE

		// INI write restrictions in Restricted/Standard mode
		const UAntigravityDeveloperSettings* Settings = UAntigravityDeveloperSettings::Get();
		if (Settings && !Settings->IsIniSectionAllowed(Section))
		{
			Result.Errors.Add(FString::Printf(
				TEXT("INI write to section '%s' is blocked in Restricted/Standard security mode. Only /Script/Antigravity.* sections are writable. Switch to Full Access mode for full access."),
				*Section));
			return Result;
		}

		// Determine which ini to target
		FString TargetIni = GEngineIni;
		if (ConfigFile.Contains(TEXT("Game"))) TargetIni = GGameIni;
		else if (ConfigFile.Contains(TEXT("Input"))) TargetIni = GInputIni;
		else if (ConfigFile.Contains(TEXT("Editor"))) TargetIni = GEditorIni;

		// Use Reflection + PostEditChangeProperty instead of raw GConfig
		bool bUsedReflection = false;

		if (FModuleManager::Get().IsModuleLoaded(TEXT("Settings")))
		{
			FString ClassName = FPackageName::ObjectPathToObjectName(Section);
			UClass* SettingsClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);

			if (SettingsClass)
			{
				UObject* SettingsObject = SettingsClass->GetDefaultObject();
				if (SettingsObject)
				{
					FProperty* Prop = SettingsClass->FindPropertyByName(FName(*Key));
					if (Prop)
					{
						SettingsObject->Modify();
						void* PropAddr = Prop->ContainerPtrToValuePtr<void>(SettingsObject);
						Prop->ImportText_Direct(*Value, PropAddr, SettingsObject, PPF_None);
						FPropertyChangedEvent ChangedEvent(Prop);
						SettingsObject->PostEditChangeProperty(ChangedEvent);
						SettingsObject->SaveConfig();
						bUsedReflection = true;

						UE_LOG(LogAntigravity, Log, TEXT("SettingsActions: Set %s.%s = %s via Reflection+PostEditChangeProperty"),
							*Section, *Key, *Value);
					}
				}
			}
		}

		// Fallback: use GConfig directly if reflection didn't work
		if (!bUsedReflection)
		{
			GConfig->SetString(*Section, *Key, *Value, TargetIni);
			GConfig->Flush(false, TargetIni);

			UE_LOG(LogAntigravity, Log, TEXT("SettingsActions: Set [%s] %s = %s via GConfig (no live UI update)"),
				*Section, *Key, *Value);
		}

		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Set [%s] %s = %s in %s%s"),
			*Section, *Key, *Value, *ConfigFile,
			bUsedReflection ? TEXT(" (live UI updated)") : TEXT(" (ini only, restart may be needed)"));
		Result.ModifiedPaths.Add(FString::Printf(TEXT("%s: [%s] %s"), *ConfigFile, *Section, *Key));
	}

	return Result;
}

// ============================================================================
// ExecuteMacroEnsureProjectPrerequisites
// ============================================================================
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

FAntigravityActionResult FAntigravitySettingsActions::ExecuteMacroEnsureProjectPrerequisites(const TSharedRef<FJsonObject>& Params, FAntigravityActionResult& Result)
{
	TArray<FString> PluginsToEnable;
	if (Params->HasTypedField<EJson::Array>(TEXT("plugins")))
	{
		for (const TSharedPtr<FJsonValue>& Val : Params->GetArrayField(TEXT("plugins")))
		{
			PluginsToEnable.Add(Val->AsString());
		}
	}

	FString UProjectFilePath = FPaths::GetProjectFilePath();
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *UProjectFilePath))
	{
		Result.Errors.Add(TEXT("Failed to load .uproject file."));
		return Result;
	}

	TSharedPtr<FJsonObject> UProjectJson;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, UProjectJson) || !UProjectJson.IsValid())
	{
		Result.Errors.Add(TEXT("Failed to parse .uproject file as JSON."));
		return Result;
	}

	TArray<TSharedPtr<FJsonValue>> PluginsArray;
	if (UProjectJson->HasTypedField<EJson::Array>(TEXT("Plugins")))
	{
		PluginsArray = UProjectJson->GetArrayField(TEXT("Plugins"));
	}

	int32 EnabledCount = 0;
	for (const FString& PluginName : PluginsToEnable)
	{
		bool bFound = false;
		for (TSharedPtr<FJsonValue>& PluginObjVal : PluginsArray)
		{
			TSharedPtr<FJsonObject> PluginObj = PluginObjVal->AsObject();
			if (PluginObj.IsValid() && PluginObj->GetStringField(TEXT("Name")) == PluginName)
			{
				bFound = true;
				if (!PluginObj->GetBoolField(TEXT("Enabled")))
				{
					PluginObj->SetBoolField(TEXT("Enabled"), true);
					EnabledCount++;
				}
				break;
			}
		}

		if (!bFound)
		{
			TSharedPtr<FJsonObject> NewPlugin = MakeShared<FJsonObject>();
			NewPlugin->SetStringField(TEXT("Name"), PluginName);
			NewPlugin->SetBoolField(TEXT("Enabled"), true);
			PluginsArray.Add(MakeShared<FJsonValueObject>(NewPlugin));
			EnabledCount++;
		}
	}

	if (EnabledCount > 0)
	{
		UProjectJson->SetArrayField(TEXT("Plugins"), PluginsArray);

		FString OutputString;
		auto Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputString);
		FJsonSerializer::Serialize(UProjectJson.ToSharedRef(), Writer);

		if (!FFileHelper::SaveStringToFile(OutputString, *UProjectFilePath))
		{
			Result.Errors.Add(TEXT("Failed to save modified .uproject file."));
			return Result;
		}

		Result.ResultMessage = FString::Printf(TEXT("Enabled %d missing plugins. [AI HINT: You must inform the user to restart the Editor for these plugins to load.]"), EnabledCount);
	}
	else
	{
		Result.ResultMessage = TEXT("All requested plugins are already enabled.");
	}

	Result.bSuccess = true;
	return Result;
}
