// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Settings/AgentFrameworkSettingsActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkSettings.h"
#include "AgentFrameworkActionUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

FAgentFrameworkSettingsActions::FAgentFrameworkSettingsActions() {}
FAgentFrameworkSettingsActions::~FAgentFrameworkSettingsActions() {}

FName FAgentFrameworkSettingsActions::GetActionName() const
{
	return FName(TEXT("Settings"));
}

TArray<FString> FAgentFrameworkSettingsActions::GetSupportedToolNames() const
{
	return {
		TEXT("read_config_value"),
		TEXT("write_config_value"),
		TEXT("macro_ensure_project_prerequisites"),
		TEXT("get_plugin_settings"),
		TEXT("list_config_sections"),
		TEXT("read_config_section")
	};
}

bool FAgentFrameworkSettingsActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, OutErrors, false);

	if (ToolName == TEXT("read_config_value"))
	{
		FString Section, Key;
		bool bValidSection = UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("section"), Section, OutErrors, true);
		bool bValidKey = UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("key"), Key, OutErrors, true);
		return bValidSection && bValidKey;
	}
	else if (ToolName == TEXT("write_config_value"))
	{
		FString Section, Key;
		bool bValidSection = UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("section"), Section, OutErrors, true);
		bool bValidKey = UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("key"), Key, OutErrors, true);
		if (!Params->HasField(TEXT("value")))
		{
			OutErrors.Add(TEXT("Missing required field for write_config_value: 'value'"));
			return false;
		}
		return bValidSection && bValidKey;
	}
	else if (ToolName == TEXT("macro_ensure_project_prerequisites"))
	{
		TArray<FString> PluginsArray;
		return UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("plugins"), PluginsArray, OutErrors, true);
	}
	else if (ToolName == TEXT("get_plugin_settings"))
	{
		return true;
	}
	else if (ToolName == TEXT("list_config_sections"))
	{
		FString ConfigFile;
		return UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("config_file"), ConfigFile, OutErrors, true);
	}
	else if (ToolName == TEXT("read_config_section"))
	{
		FString Section;
		return UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("section"), Section, OutErrors, true);
	}

	return true;
}

FString FAgentFrameworkSettingsActions::ResolveTargetIni(const FString& ConfigFile) const
{
	if (ConfigFile.Contains(TEXT("Game")))
	{
		return GGameIni;
	}
	else if (ConfigFile.Contains(TEXT("Input")))
	{
		return GInputIni;
	}
	else if (ConfigFile.Contains(TEXT("Editor")))
	{
		return GEditorIni;
	}
	else if (ConfigFile.Contains(TEXT("Scalability")))
	{
		return GScalabilityIni;
	}
	return GEngineIni;
}

FAgentFrameworkActionResult FAgentFrameworkSettingsActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, Result.Errors, false);

	bool bIsWrite = (ToolName == TEXT("write_config_value"));
	bool bNeedsTransaction = bIsWrite;

	TOptional<FScopedTransaction> Transaction;
	if (bNeedsTransaction)
	{
		Transaction.Emplace(FText::FromString(TEXT("AgentFramework Settings Action")));
	}

	if (ToolName == TEXT("read_config_value"))
	{
		Result = ExecuteReadConfigValue(Params);
	}
	else if (ToolName == TEXT("write_config_value"))
	{
		Result = ExecuteWriteConfigValue(Params);
	}
	else if (ToolName == TEXT("macro_ensure_project_prerequisites"))
	{
		Result = ExecuteMacroEnsureProjectPrerequisites(Params);
	}
	else if (ToolName == TEXT("get_plugin_settings"))
	{
		Result = ExecuteGetPluginSettings(Params);
	}
	else if (ToolName == TEXT("list_config_sections"))
	{
		Result = ExecuteListConfigSections(Params);
	}
	else if (ToolName == TEXT("read_config_section"))
	{
		Result = ExecuteReadConfigSection(Params);
	}
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown tool name '%s' for Settings actions."), *ToolName));
	}

	if (Transaction.IsSet() && !Result.bSuccess)
	{
		Transaction->Cancel();
	}

	if (Result.bSuccess && bIsWrite)
	{
		PlaySuccessSound();
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkSettingsActions::ExecuteReadConfigValue(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ConfigFile, Section, Key;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("config_file"), ConfigFile, Result.Errors, false);
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("section"), Section, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("key"), Key, Result.Errors, true))
	{
		return Result;
	}

	if (!GConfig)
	{
		Result.Errors.Add(TEXT("GConfig is unavailable. Cannot perform config operation."));
		return Result;
	}

	FString TargetIni = ResolveTargetIni(ConfigFile);
	FString ReadValue;

	if (GConfig->GetString(*Section, *Key, ReadValue, TargetIni))
	{
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("[%s] %s = %s (from %s)"), *Section, *Key, *ReadValue, ConfigFile.IsEmpty() ? TEXT("Engine") : *ConfigFile);

		TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
		ResponseData->SetStringField(TEXT("section"), Section);
		ResponseData->SetStringField(TEXT("key"), Key);
		ResponseData->SetStringField(TEXT("value"), ReadValue);
		ResponseData->SetStringField(TEXT("config_file"), ConfigFile.IsEmpty() ? TEXT("DefaultEngine") : ConfigFile);
	}
	else
	{
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Setting not found: [%s] %s in %s. The key does not exist in this config file."), *Section, *Key, ConfigFile.IsEmpty() ? TEXT("Engine") : *ConfigFile);

		TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
		ResponseData->SetStringField(TEXT("section"), Section);
		ResponseData->SetStringField(TEXT("key"), Key);
		ResponseData->SetBoolField(TEXT("found"), false);
	}

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkSettingsActions::ExecuteWriteConfigValue(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ConfigFile, Section, Key, Value;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("config_file"), ConfigFile, Result.Errors, false);
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("section"), Section, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("key"), Key, Result.Errors, true))
	{
		return Result;
	}

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
	else
	{
		Result.Errors.Add(TEXT("Parameter 'value' is required for write_config_value."));
		return Result;
	}

	if (!GConfig)
	{
		Result.Errors.Add(TEXT("GConfig is unavailable. Cannot perform config operation."));
		return Result;
	}

	// Security check for Restricted/Standard mode
	const UAgentFrameworkDeveloperSettings* Settings = UAgentFrameworkDeveloperSettings::Get();
	if (IsValid(Settings) && !Settings->IsIniSectionAllowed(Section))
	{
		Result.Errors.Add(FString::Printf(
			TEXT("INI write to section '%s' is blocked in Restricted/Standard security mode. Only /Script/AgentFramework.* sections are writable. Switch to Full Access mode for full access."),
			*Section));
		return Result;
	}

	FString TargetIni = ResolveTargetIni(ConfigFile);
	bool bUsedReflection = false;

	if (FModuleManager::Get().IsModuleLoaded(TEXT("Settings")))
	{
		FString ClassName = FPackageName::ObjectPathToObjectName(Section);
		UClass* SettingsClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None);

		if (IsValid(SettingsClass))
		{
			UObject* SettingsObject = SettingsClass->GetDefaultObject();
			if (IsValid(SettingsObject))
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

					UE_LOG(LogAgentFramework, Log, TEXT("SettingsActions: Set %s.%s = %s via Reflection+PostEditChangeProperty"),
						*Section, *Key, *Value);
				}
			}
		}
	}

	if (!bUsedReflection)
	{
		GConfig->SetString(*Section, *Key, *Value, TargetIni);
		GConfig->Flush(false, TargetIni);

		UE_LOG(LogAgentFramework, Log, TEXT("SettingsActions: Set [%s] %s = %s via GConfig"),
			*Section, *Key, *Value);
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Set [%s] %s = %s in %s%s"),
		*Section, *Key, *Value, ConfigFile.IsEmpty() ? TEXT("Engine") : *ConfigFile,
		bUsedReflection ? TEXT(" (live UI updated)") : TEXT(" (ini only, restart may be needed)"));
	Result.ModifiedPaths.Add(FString::Printf(TEXT("%s: [%s] %s"), ConfigFile.IsEmpty() ? TEXT("Engine") : *ConfigFile, *Section, *Key));

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("section"), Section);
	ResponseData->SetStringField(TEXT("key"), Key);
	ResponseData->SetStringField(TEXT("value"), Value);
	ResponseData->SetBoolField(TEXT("used_reflection"), bUsedReflection);

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkSettingsActions::ExecuteMacroEnsureProjectPrerequisites(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	TArray<FString> PluginsToEnable;
	if (!UAgentFrameworkActionUtils::TryGetStringArrayParam(Params, TEXT("plugins"), PluginsToEnable, Result.Errors, true))
	{
		return Result;
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
			if (PluginObjVal.IsValid())
			{
				TSharedPtr<FJsonObject> PluginObj = PluginObjVal->AsObject();
				if (PluginObj.IsValid() && PluginObj->GetStringField(TEXT("Name")) == PluginName)
				{
					bFound = true;
					if (!PluginObj->HasField(TEXT("Enabled")) || !PluginObj->GetBoolField(TEXT("Enabled")))
					{
						PluginObj->SetBoolField(TEXT("Enabled"), true);
						EnabledCount++;
					}
					break;
				}
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

FAgentFrameworkActionResult FAgentFrameworkSettingsActions::ExecuteGetPluginSettings(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	const UAgentFrameworkDeveloperSettings* Settings = UAgentFrameworkDeveloperSettings::Get();
	if (!IsValid(Settings))
	{
		Result.Errors.Add(TEXT("Failed to access UAgentFrameworkDeveloperSettings object."));
		return Result;
	}

	FString SecurityModeStr;
	switch (Settings->SecurityMode)
	{
	case EAgentFrameworkSecurityMode::Restricted:
		SecurityModeStr = TEXT("Restricted");
		break;
	case EAgentFrameworkSecurityMode::Standard:
		SecurityModeStr = TEXT("Standard");
		break;
	case EAgentFrameworkSecurityMode::FullAccess:
		SecurityModeStr = TEXT("FullAccess");
		break;
	default:
		SecurityModeStr = TEXT("Unknown");
		break;
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("security_mode"), SecurityModeStr);
	ResponseData->SetBoolField(TEXT("override_default_protected_paths"), Settings->bOverrideDefaultProtectedPaths);

	TArray<TSharedPtr<FJsonValue>> ProtectedPathsArray;
	for (const FString& Path : Settings->AdditionalProtectedPaths)
	{
		ProtectedPathsArray.Add(MakeShared<FJsonValueString>(Path));
	}
	ResponseData->SetArrayField(TEXT("additional_protected_paths"), ProtectedPathsArray);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("AgentFramework Plugin Settings: SecurityMode=%s, AdditionalProtectedPathsCount=%d, OverrideDefaultProtectedPaths=%s"),
		*SecurityModeStr, Settings->AdditionalProtectedPaths.Num(), Settings->bOverrideDefaultProtectedPaths ? TEXT("true") : TEXT("false"));

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkSettingsActions::ExecuteListConfigSections(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ConfigFile;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("config_file"), ConfigFile, Result.Errors, true))
	{
		return Result;
	}

	if (!GConfig)
	{
		Result.Errors.Add(TEXT("GConfig is unavailable. Cannot list config sections."));
		return Result;
	}

	FString TargetIni = ResolveTargetIni(ConfigFile);
	TArray<FString> SectionNames;
	GConfig->GetSectionNames(TargetIni, SectionNames);

	TArray<TSharedPtr<FJsonValue>> SectionsArray;
	for (const FString& Section : SectionNames)
	{
		SectionsArray.Add(MakeShared<FJsonValueString>(Section));
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("config_file"), ConfigFile);
	ResponseData->SetNumberField(TEXT("section_count"), SectionNames.Num());
	ResponseData->SetArrayField(TEXT("sections"), SectionsArray);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Found %d sections in config file '%s'"), SectionNames.Num(), *ConfigFile);

	return Result;
}

FAgentFrameworkActionResult FAgentFrameworkSettingsActions::ExecuteReadConfigSection(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString ConfigFile, Section;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("config_file"), ConfigFile, Result.Errors, false);
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("section"), Section, Result.Errors, true))
	{
		return Result;
	}

	if (!GConfig)
	{
		Result.Errors.Add(TEXT("GConfig is unavailable. Cannot read config section."));
		return Result;
	}

	FString TargetIni = ResolveTargetIni(ConfigFile);
	const FConfigSection* ConfigSec = GConfig->GetSection(*Section, false, TargetIni);

	TSharedPtr<FJsonObject> SectionData = MakeShared<FJsonObject>();
	int32 KeyCount = 0;

	if (ConfigSec)
	{
		for (FConfigSection::TConstIterator It(*ConfigSec); It; ++It)
		{
			SectionData->SetStringField(It.Key().ToString(), It.Value().GetValue());
			KeyCount++;
		}
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("section"), Section);
	ResponseData->SetStringField(TEXT("config_file"), ConfigFile.IsEmpty() ? TEXT("Engine") : *ConfigFile);
	ResponseData->SetNumberField(TEXT("key_count"), KeyCount);
	ResponseData->SetObjectField(TEXT("key_values"), SectionData);

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(TEXT("Read %d keys from section '[%s]' in %s"), KeyCount, *Section, ConfigFile.IsEmpty() ? TEXT("Engine") : *ConfigFile);

	return Result;
}

void FAgentFrameworkSettingsActions::PlaySuccessSound()
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


