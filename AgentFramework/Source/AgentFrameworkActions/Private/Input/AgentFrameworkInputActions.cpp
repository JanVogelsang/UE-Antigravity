// Copyright 2026 AgentFramework. All Rights Reserved.

#include "Input/AgentFrameworkInputActions.h"
#include "AgentFrameworkCoreModule.h"
#include "AgentFrameworkActionUtils.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Curves/CurveFloat.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "ScopedTransaction.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Sound/SoundBase.h"
#endif

FAgentFrameworkInputActions::FAgentFrameworkInputActions() {}
FAgentFrameworkInputActions::~FAgentFrameworkInputActions() {}
FName FAgentFrameworkInputActions::GetActionName() const { return FName(TEXT("Input")); }

TArray<FString> FAgentFrameworkInputActions::GetSupportedToolNames() const
{
	return {
		TEXT("create_input_action"),
		TEXT("create_input_mapping_context"),
		TEXT("add_input_mapping"),
		TEXT("configure_input_mapping_modifiers_triggers")
	};
}

bool FAgentFrameworkInputActions::ValidateParams(const TSharedRef<FJsonObject>& Params, TArray<FString>& OutErrors) const
{
	FString ToolName;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, OutErrors, false);

	if (ToolName == TEXT("create_input_action") || ToolName == TEXT("create_input_mapping_context"))
	{
		FString AssetPath;
		return UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, OutErrors, true);
	}
	else if (ToolName == TEXT("add_input_mapping"))
	{
		FString IMCPath, IAPath, KeyName;
		bool bValid = true;
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("mapping_context_path"), IMCPath, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action_path"), IAPath, OutErrors, true);
		bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("key"), KeyName, OutErrors, true);
		return bValid;
	}
	else if (ToolName == TEXT("configure_input_mapping_modifiers_triggers"))
	{
		FString IMCPath, IAPath, KeyName;
		bool bHasIMC = UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("mapping_context_path"), IMCPath, OutErrors, false) ||
		               UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("ContextAsset"), IMCPath, OutErrors, false);
		bool bHasIA = UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action_path"), IAPath, OutErrors, false) ||
		              UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("InputActionAsset"), IAPath, OutErrors, false);
		bool bHasKey = UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("key"), KeyName, OutErrors, false) ||
		               UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("Key"), KeyName, OutErrors, false);
		if (!bHasIMC) OutErrors.Add(TEXT("Missing required parameter: 'mapping_context_path' or 'ContextAsset'"));
		if (!bHasIA) OutErrors.Add(TEXT("Missing required parameter: 'action_path' or 'InputActionAsset'"));
		if (!bHasKey) OutErrors.Add(TEXT("Missing required parameter: 'key' or 'Key'"));
		return bHasIMC && bHasIA && bHasKey;
	}

	return true;
}

// Helper: resolve EInputActionValueType from string
static EInputActionValueType ParseValueType(const FString& TypeStr)
{
	if (TypeStr.Equals(TEXT("Axis1D"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		return EInputActionValueType::Axis1D;
	if (TypeStr.Equals(TEXT("Axis2D"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("Vector2D"), ESearchCase::IgnoreCase))
		return EInputActionValueType::Axis2D;
	if (TypeStr.Equals(TEXT("Axis3D"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
		return EInputActionValueType::Axis3D;
	// Default: Boolean/Digital
	return EInputActionValueType::Boolean;
}

// Helper: save a newly created asset package
static bool SaveAssetPackage(UPackage* Package, UObject* Asset, const FString& AssetPath)
{
	if (!IsValid(Package) || !IsValid(Asset))
	{
		return false;
	}

	Asset->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Asset);

	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		AssetPath, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
}

// Helper: find FKey from string name
static FKey ParseKeyName(const FString& KeyName)
{
	// Use TCHAR* constructor to avoid most-vexing-parse with FName()
	FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		UE_LOG(LogAgentFramework, Warning, TEXT("InputActions: Key '%s' not recognized, using as-is"), *KeyName);
	}
	return Key;
}

void FAgentFrameworkInputActions::PlaySuccessSound()
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

FAgentFrameworkActionResult FAgentFrameworkInputActions::ExecuteAction(const TSharedRef<FJsonObject>& Params)
{
	FScopedTransaction Transaction(FText::FromString(TEXT("AgentFramework Enhanced Input Action")));

	// Dispatch by _tool_name (injected by ActionRouter with underscore prefix)
	FString ToolName;
	TArray<FString> TempErrors;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("_tool_name"), ToolName, TempErrors, false);

	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	if (ToolName == TEXT("create_input_action"))
		Result = ExecuteCreateInputAction(Params);
	else if (ToolName == TEXT("create_input_mapping_context"))
		Result = ExecuteCreateInputMappingContext(Params);
	else if (ToolName == TEXT("add_input_mapping"))
		Result = ExecuteAddInputMapping(Params);
	else if (ToolName == TEXT("configure_input_mapping_modifiers_triggers"))
		Result = ExecuteConfigureInputMappingModifiersTriggers(Params);
	else
	{
		Result.Errors.Add(FString::Printf(TEXT("Unknown tool: %s"), *ToolName));
	}

	if (Result.bSuccess)
	{
		PlaySuccessSound();
	}
	else
	{
		Transaction.Cancel();
	}

	return Result;
}

// ============================================================
// create_input_action
// ============================================================
FAgentFrameworkActionResult FAgentFrameworkInputActions::ExecuteCreateInputAction(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	FString ValueTypeStr;
	UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("value_type"), ValueTypeStr, Result.Errors, false);

	// Check if asset already exists
	if (IsValid(FindObject<UInputAction>(nullptr, *AssetPath)))
	{
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Input Action already exists at %s"), *AssetPath);
		return Result;
	}

	// Extract package path and asset name
	FString PackagePath = AssetPath;
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!IsValid(Package))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create package: %s"), *PackagePath));
		return Result;
	}

	UInputAction* NewAction = NewObject<UInputAction>(Package, FName(*AssetName),
		RF_Public | RF_Standalone | RF_Transactional);
	if (!IsValid(NewAction))
	{
		Result.Errors.Add(TEXT("Failed to create UInputAction object"));
		return Result;
	}

	// Set value type
	NewAction->ValueType = ParseValueType(ValueTypeStr);

	// Optional: consume input
	bool bConsumeInput = true;
	UAgentFrameworkActionUtils::TryGetBoolParam(Params, TEXT("consume_input"), bConsumeInput, Result.Errors, false);
	NewAction->bConsumeInput = bConsumeInput;

	// Save
	if (!SaveAssetPackage(Package, NewAction, PackagePath))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to save Input Action to disk: %s"), *PackagePath));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(
		TEXT("Created Input Action '%s' (ValueType=%s) at %s"),
		*AssetName,
		ValueTypeStr.IsEmpty() ? TEXT("Boolean") : *ValueTypeStr,
		*AssetPath);
	Result.ModifiedPaths.Add(AssetPath);

	UE_LOG(LogAgentFramework, Log, TEXT("InputActions: Created IA '%s' at %s"), *AssetName, *AssetPath);
	return Result;
}

// ============================================================
// create_input_mapping_context
// ============================================================
FAgentFrameworkActionResult FAgentFrameworkInputActions::ExecuteCreateInputMappingContext(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString AssetPath;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("asset_path"), AssetPath, Result.Errors, true))
	{
		return Result;
	}

	// Check if asset already exists
	if (IsValid(FindObject<UInputMappingContext>(nullptr, *AssetPath)))
	{
		Result.bSuccess = true;
		Result.ResultMessage = FString::Printf(TEXT("Input Mapping Context already exists at %s"), *AssetPath);
		return Result;
	}

	FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*AssetPath);
	if (!IsValid(Package))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to create package: %s"), *AssetPath));
		return Result;
	}

	UInputMappingContext* NewIMC = NewObject<UInputMappingContext>(Package, FName(*AssetName),
		RF_Public | RF_Standalone | RF_Transactional);
	if (!IsValid(NewIMC))
	{
		Result.Errors.Add(TEXT("Failed to create UInputMappingContext object"));
		return Result;
	}

	// Save
	if (!SaveAssetPackage(Package, NewIMC, AssetPath))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to save IMC to disk: %s"), *AssetPath));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(
		TEXT("Created Input Mapping Context '%s' at %s"),
		*AssetName, *AssetPath);
	Result.ModifiedPaths.Add(AssetPath);

	UE_LOG(LogAgentFramework, Log, TEXT("InputActions: Created IMC '%s' at %s"), *AssetName, *AssetPath);
	return Result;
}

// ============================================================
// add_input_mapping
// ============================================================
FAgentFrameworkActionResult FAgentFrameworkInputActions::ExecuteAddInputMapping(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString IMCPath, IAPath, KeyName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("mapping_context_path"), IMCPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action_path"), IAPath, Result.Errors, true) ||
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("key"), KeyName, Result.Errors, true))
	{
		return Result;
	}

	// Load assets
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *IMCPath);
	if (!IsValid(IMC))
	{
		// Try with _C suffix stripped or added
		IMC = LoadObject<UInputMappingContext>(nullptr, *(IMCPath + TEXT(".") + FPackageName::GetLongPackageAssetName(IMCPath)));
		if (!IsValid(IMC))
		{
			Result.Errors.Add(FString::Printf(TEXT("Could not load Input Mapping Context: %s"), *IMCPath));
			return Result;
		}
	}

	UInputAction* IA = LoadObject<UInputAction>(nullptr, *IAPath);
	if (!IsValid(IA))
	{
		IA = LoadObject<UInputAction>(nullptr, *(IAPath + TEXT(".") + FPackageName::GetLongPackageAssetName(IAPath)));
		if (!IsValid(IA))
		{
			Result.Errors.Add(FString::Printf(TEXT("Could not load Input Action: %s"), *IAPath));
			return Result;
		}
	}

	FKey Key = ParseKeyName(KeyName);

	// Add the mapping
	IMC->Modify();
	FEnhancedActionKeyMapping& Mapping = IMC->MapKey(IA, Key);

	// Parse optional modifiers
	const TArray<TSharedPtr<FJsonValue>>* ModifiersArray = nullptr;
	UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("modifiers"), ModifiersArray, Result.Errors, false);
	if (ModifiersArray)
	{
		for (const TSharedPtr<FJsonValue>& ModVal : *ModifiersArray)
		{
			if (!ModVal.IsValid()) continue;
			FString ModName = ModVal->AsString();

			UInputModifier* NewMod = nullptr;
			if (ModName.Equals(TEXT("Negate"), ESearchCase::IgnoreCase))
			{
				NewMod = NewObject<UInputModifierNegate>();
			}
			else if (ModName.Equals(TEXT("SwizzleInputAxisValues"), ESearchCase::IgnoreCase) ||
					 ModName.Equals(TEXT("Swizzle"), ESearchCase::IgnoreCase))
			{
				NewMod = NewObject<UInputModifierSwizzleAxis>();
			}
			else if (ModName.Equals(TEXT("DeadZone"), ESearchCase::IgnoreCase))
			{
				NewMod = NewObject<UInputModifierDeadZone>();
			}
			else if (ModName.Equals(TEXT("Scalar"), ESearchCase::IgnoreCase))
			{
				NewMod = NewObject<UInputModifierScalar>();
			}
			else
			{
				UE_LOG(LogAgentFramework, Warning, TEXT("InputActions: Unknown modifier '%s', skipping"), *ModName);
			}

			if (IsValid(NewMod))
			{
				Mapping.Modifiers.Add(NewMod);
			}
		}
	}

	// Parse optional triggers
	const TArray<TSharedPtr<FJsonValue>>* TriggersArray = nullptr;
	UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("triggers"), TriggersArray, Result.Errors, false);
	if (TriggersArray)
	{
		for (const TSharedPtr<FJsonValue>& TrigVal : *TriggersArray)
		{
			if (!TrigVal.IsValid()) continue;
			FString TrigName = TrigVal->AsString();

			UInputTrigger* NewTrig = nullptr;
			if (TrigName.Equals(TEXT("Pressed"), ESearchCase::IgnoreCase) ||
				TrigName.Equals(TEXT("Down"), ESearchCase::IgnoreCase))
			{
				NewTrig = NewObject<UInputTriggerPressed>();
			}
			else if (TrigName.Equals(TEXT("Released"), ESearchCase::IgnoreCase))
			{
				NewTrig = NewObject<UInputTriggerReleased>();
			}
			else if (TrigName.Equals(TEXT("Hold"), ESearchCase::IgnoreCase))
			{
				NewTrig = NewObject<UInputTriggerHold>();
			}
			else if (TrigName.Equals(TEXT("Tap"), ESearchCase::IgnoreCase))
			{
				NewTrig = NewObject<UInputTriggerTap>();
			}
			else if (TrigName.Equals(TEXT("Pulse"), ESearchCase::IgnoreCase))
			{
				NewTrig = NewObject<UInputTriggerPulse>();
			}
			else if (!TrigName.Equals(TEXT("default"), ESearchCase::IgnoreCase) && !TrigName.IsEmpty())
			{
				UE_LOG(LogAgentFramework, Warning, TEXT("InputActions: Unknown trigger '%s', skipping"), *TrigName);
			}

			if (IsValid(NewTrig))
			{
				Mapping.Triggers.Add(NewTrig);
			}
		}
	}

	// CRITICAL FIX: UE5 requires every key mapping to have at least one Input Trigger.
	// "There cannot be a null Input Trigger on a key mapping" warnings break asset validation.
	// If no valid triggers were specified (or AI passed "default"), add UInputTriggerPressed.
	if (Mapping.Triggers.Num() == 0)
	{
		UInputTriggerPressed* DefaultTrigger = NewObject<UInputTriggerPressed>();
		if (IsValid(DefaultTrigger))
		{
			Mapping.Triggers.Add(DefaultTrigger);
			UE_LOG(LogAgentFramework, Log, TEXT("InputActions: No triggers specified — defaulting to Pressed trigger."));
		}
	}

	// Save the modified IMC
	IMC->MarkPackageDirty();
	UPackage* Package = IMC->GetOutermost();
	if (!IsValid(Package))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to get outermost package for IMC: %s"), *IMCPath));
		return Result;
	}

	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, IMC, *PackageFileName, SaveArgs))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to save modified IMC to disk: %s"), *IMCPath));
		return Result;
	}

	// Build result description
	FString ModsDesc = TEXT("none");
	if (ModifiersArray && ModifiersArray->Num() > 0)
	{
		TArray<FString> ModNames;
		for (const TSharedPtr<FJsonValue>& M : *ModifiersArray)
		{
			if (M.IsValid())
			{
				ModNames.Add(M->AsString());
			}
		}
		ModsDesc = FString::Join(ModNames, TEXT(", "));
	}

	FString TrigsDesc = TEXT("default");
	if (TriggersArray && TriggersArray->Num() > 0)
	{
		TArray<FString> TrigNames;
		for (const TSharedPtr<FJsonValue>& T : *TriggersArray)
		{
			if (T.IsValid())
			{
				TrigNames.Add(T->AsString());
			}
		}
		TrigsDesc = FString::Join(TrigNames, TEXT(", "));
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(
		TEXT("Added mapping: %s -> %s (Key=%s, Modifiers=[%s], Triggers=[%s])"),
		*IA->GetName(), *IMC->GetName(), *KeyName, *ModsDesc, *TrigsDesc);
	Result.ModifiedPaths.Add(IMCPath);

	UE_LOG(LogAgentFramework, Log, TEXT("InputActions: Added mapping %s -> %s (Key=%s)"),
		*IA->GetName(), *IMC->GetName(), *KeyName);
	return Result;
}

// ============================================================
// configure_input_mapping_modifiers_triggers
// ============================================================
FAgentFrameworkActionResult FAgentFrameworkInputActions::ExecuteConfigureInputMappingModifiersTriggers(const TSharedRef<FJsonObject>& Params)
{
	FAgentFrameworkActionResult Result;
	Result.bSuccess = false;

	FString IMCPath, IAPath, KeyName;
	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("mapping_context_path"), IMCPath, Result.Errors, false) &&
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("ContextAsset"), IMCPath, Result.Errors, false))
	{
		Result.Errors.Add(TEXT("Missing required parameter: 'mapping_context_path' or 'ContextAsset'"));
		return Result;
	}

	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action_path"), IAPath, Result.Errors, false) &&
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("InputActionAsset"), IAPath, Result.Errors, false))
	{
		Result.Errors.Add(TEXT("Missing required parameter: 'action_path' or 'InputActionAsset'"));
		return Result;
	}

	if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("key"), KeyName, Result.Errors, false) &&
		!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("Key"), KeyName, Result.Errors, false))
	{
		Result.Errors.Add(TEXT("Missing required parameter: 'key' or 'Key'"));
		return Result;
	}

	// Load assets
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *IMCPath);
	if (!IsValid(IMC))
	{
		IMC = LoadObject<UInputMappingContext>(nullptr, *(IMCPath + TEXT(".") + FPackageName::GetLongPackageAssetName(IMCPath)));
		if (!IsValid(IMC))
		{
			Result.Errors.Add(FString::Printf(TEXT("Could not load Input Mapping Context: %s"), *IMCPath));
			return Result;
		}
	}

	UInputAction* IA = LoadObject<UInputAction>(nullptr, *IAPath);
	if (!IsValid(IA))
	{
		IA = LoadObject<UInputAction>(nullptr, *(IAPath + TEXT(".") + FPackageName::GetLongPackageAssetName(IAPath)));
		if (!IsValid(IA))
		{
			Result.Errors.Add(FString::Printf(TEXT("Could not load Input Action: %s"), *IAPath));
			return Result;
		}
	}

	FKey Key = ParseKeyName(KeyName);

	// Modify IMC and resolve/add mapping for Key & Action
	IMC->Modify();
	FEnhancedActionKeyMapping& Mapping = IMC->MapKey(IA, Key);

	// Clear existing modifiers & triggers on this target mapping
	Mapping.Modifiers.Empty();
	Mapping.Triggers.Empty();

	int32 AppliedModifiersCount = 0;
	int32 AppliedTriggersCount = 0;

	// Parse modifiers array
	const TArray<TSharedPtr<FJsonValue>>* ModifiersArray = nullptr;
	if (UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("modifiers"), ModifiersArray, Result.Errors, false) ||
		UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("Modifiers"), ModifiersArray, Result.Errors, false))
	{
		if (ModifiersArray)
		{
			for (const TSharedPtr<FJsonValue>& ModVal : *ModifiersArray)
			{
				if (!ModVal.IsValid()) continue;

				FString ModType;
				TSharedPtr<FJsonObject> ModObj;

				if (ModVal->Type == EJson::String)
				{
					ModType = ModVal->AsString();
				}
				else if (ModVal->Type == EJson::Object)
				{
					ModObj = ModVal->AsObject();
					if (!UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("type"), ModType, Result.Errors, false) &&
						!UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("Type"), ModType, Result.Errors, false))
					{
						UE_LOG(LogAgentFramework, Warning, TEXT("InputActions: Modifier object missing 'type' or 'Type' field, skipping"));
						continue;
					}
				}

				UInputModifier* NewMod = nullptr;

				if (ModType.Equals(TEXT("Negate"), ESearchCase::IgnoreCase))
				{
					UInputModifierNegate* NegateMod = NewObject<UInputModifierNegate>(IMC);
					if (ModObj.IsValid())
					{
						bool bX = true, bY = true, bZ = true;
						UAgentFrameworkActionUtils::TryGetBoolParam(ModObj.ToSharedRef(), TEXT("bX"), bX, Result.Errors, false);
						UAgentFrameworkActionUtils::TryGetBoolParam(ModObj.ToSharedRef(), TEXT("b_x"), bX, Result.Errors, false);
						UAgentFrameworkActionUtils::TryGetBoolParam(ModObj.ToSharedRef(), TEXT("bY"), bY, Result.Errors, false);
						UAgentFrameworkActionUtils::TryGetBoolParam(ModObj.ToSharedRef(), TEXT("b_y"), bY, Result.Errors, false);
						UAgentFrameworkActionUtils::TryGetBoolParam(ModObj.ToSharedRef(), TEXT("bZ"), bZ, Result.Errors, false);
						UAgentFrameworkActionUtils::TryGetBoolParam(ModObj.ToSharedRef(), TEXT("b_z"), bZ, Result.Errors, false);
						NegateMod->bX = bX;
						NegateMod->bY = bY;
						NegateMod->bZ = bZ;
					}
					NewMod = NegateMod;
				}
				else if (ModType.Equals(TEXT("SwizzleAxis"), ESearchCase::IgnoreCase) ||
						 ModType.Equals(TEXT("SwizzleInputAxisValues"), ESearchCase::IgnoreCase) ||
						 ModType.Equals(TEXT("Swizzle"), ESearchCase::IgnoreCase))
				{
					UInputModifierSwizzleAxis* SwizzleMod = NewObject<UInputModifierSwizzleAxis>(IMC);
					if (ModObj.IsValid())
					{
						FString OrderStr;
						if (UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("order"), OrderStr, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("Order"), OrderStr, Result.Errors, false))
						{
							if (OrderStr.Equals(TEXT("ZYX"), ESearchCase::IgnoreCase))
								SwizzleMod->Order = EInputAxisSwizzle::ZYX;
							else if (OrderStr.Equals(TEXT("XZY"), ESearchCase::IgnoreCase))
								SwizzleMod->Order = EInputAxisSwizzle::XZY;
							else if (OrderStr.Equals(TEXT("YZX"), ESearchCase::IgnoreCase))
								SwizzleMod->Order = EInputAxisSwizzle::YZX;
							else if (OrderStr.Equals(TEXT("ZXY"), ESearchCase::IgnoreCase))
								SwizzleMod->Order = EInputAxisSwizzle::ZXY;
							else
								SwizzleMod->Order = EInputAxisSwizzle::YXZ;
						}
					}
					NewMod = SwizzleMod;
				}
				else if (ModType.Equals(TEXT("Scalar"), ESearchCase::IgnoreCase))
				{
					UInputModifierScalar* ScalarMod = NewObject<UInputModifierScalar>(IMC);
					if (ModObj.IsValid())
					{
						const TSharedPtr<FJsonObject>* VecObj = nullptr;
						if (ModObj->TryGetObjectField(TEXT("scalar_vector"), VecObj) ||
							ModObj->TryGetObjectField(TEXT("ScalarVector"), VecObj) ||
							ModObj->TryGetObjectField(TEXT("scalar"), VecObj) ||
							ModObj->TryGetObjectField(TEXT("Scalar"), VecObj))
						{
							if (VecObj && VecObj->IsValid())
							{
								float X = 1.0f, Y = 1.0f, Z = 1.0f;
								double TempX = 1.0, TempY = 1.0, TempZ = 1.0;
								if ((*VecObj)->TryGetNumberField(TEXT("X"), TempX) || (*VecObj)->TryGetNumberField(TEXT("x"), TempX)) X = (float)TempX;
								if ((*VecObj)->TryGetNumberField(TEXT("Y"), TempY) || (*VecObj)->TryGetNumberField(TEXT("y"), TempY)) Y = (float)TempY;
								if ((*VecObj)->TryGetNumberField(TEXT("Z"), TempZ) || (*VecObj)->TryGetNumberField(TEXT("z"), TempZ)) Z = (float)TempZ;
								ScalarMod->Scalar = FVector(X, Y, Z);
							}
						}
						else
						{
							double SingleScalar = 1.0;
							if (ModObj->TryGetNumberField(TEXT("scalar"), SingleScalar) ||
								ModObj->TryGetNumberField(TEXT("Scalar"), SingleScalar) ||
								ModObj->TryGetNumberField(TEXT("value"), SingleScalar))
							{
								ScalarMod->Scalar = FVector((float)SingleScalar, (float)SingleScalar, (float)SingleScalar);
							}
						}
					}
					NewMod = ScalarMod;
				}
				else if (ModType.Equals(TEXT("DeadZone"), ESearchCase::IgnoreCase))
				{
					UInputModifierDeadZone* DeadZoneMod = NewObject<UInputModifierDeadZone>(IMC);
					if (ModObj.IsValid())
					{
						double Lower = 0.2, Upper = 0.9;
						if (ModObj->TryGetNumberField(TEXT("lower_threshold"), Lower) || ModObj->TryGetNumberField(TEXT("LowerThreshold"), Lower))
							DeadZoneMod->LowerThreshold = (float)Lower;
						if (ModObj->TryGetNumberField(TEXT("upper_threshold"), Upper) || ModObj->TryGetNumberField(TEXT("UpperThreshold"), Upper))
							DeadZoneMod->UpperThreshold = (float)Upper;

						FString DeadZoneTypeStr;
						if (UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("deadzone_type"), DeadZoneTypeStr, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("type"), DeadZoneTypeStr, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("Type"), DeadZoneTypeStr, Result.Errors, false))
						{
							if (DeadZoneTypeStr.Equals(TEXT("Axial"), ESearchCase::IgnoreCase))
								DeadZoneMod->Type = EDeadZoneType::Axial;
							else if (DeadZoneTypeStr.Equals(TEXT("UnscaledRadial"), ESearchCase::IgnoreCase))
								DeadZoneMod->Type = EDeadZoneType::UnscaledRadial;
							else if (DeadZoneTypeStr.Equals(TEXT("Radial"), ESearchCase::IgnoreCase))
								DeadZoneMod->Type = EDeadZoneType::Radial;
						}
					}
					NewMod = DeadZoneMod;
				}
				else if (ModType.Equals(TEXT("ResponseCurveExponential"), ESearchCase::IgnoreCase) ||
						 ModType.Equals(TEXT("ResponseCurveExp"), ESearchCase::IgnoreCase) ||
						 ModType.Equals(TEXT("Exponential"), ESearchCase::IgnoreCase))
				{
					UInputModifierResponseCurveExponential* ExpMod = NewObject<UInputModifierResponseCurveExponential>(IMC);
					if (ModObj.IsValid())
					{
						const TSharedPtr<FJsonObject>* VecObj = nullptr;
						if (ModObj->TryGetObjectField(TEXT("curve_exponent"), VecObj) ||
							ModObj->TryGetObjectField(TEXT("CurveExponent"), VecObj) ||
							ModObj->TryGetObjectField(TEXT("exponent"), VecObj) ||
							ModObj->TryGetObjectField(TEXT("Exponent"), VecObj))
						{
							if (VecObj && VecObj->IsValid())
							{
								float X = 1.0f, Y = 1.0f, Z = 1.0f;
								double TempX = 1.0, TempY = 1.0, TempZ = 1.0;
								if ((*VecObj)->TryGetNumberField(TEXT("X"), TempX) || (*VecObj)->TryGetNumberField(TEXT("x"), TempX)) X = (float)TempX;
								if ((*VecObj)->TryGetNumberField(TEXT("Y"), TempY) || (*VecObj)->TryGetNumberField(TEXT("y"), TempY)) Y = (float)TempY;
								if ((*VecObj)->TryGetNumberField(TEXT("Z"), TempZ) || (*VecObj)->TryGetNumberField(TEXT("z"), TempZ)) Z = (float)TempZ;
								ExpMod->CurveExponent = FVector(X, Y, Z);
							}
						}
					}
					NewMod = ExpMod;
				}
				else if (ModType.Equals(TEXT("ResponseCurveUser"), ESearchCase::IgnoreCase) ||
						 ModType.Equals(TEXT("ResponseCurveUserDefined"), ESearchCase::IgnoreCase) ||
						 ModType.Equals(TEXT("UserCurve"), ESearchCase::IgnoreCase) ||
						 ModType.Equals(TEXT("ResponseCurve"), ESearchCase::IgnoreCase))
				{
					UInputModifierResponseCurveUser* UserMod = NewObject<UInputModifierResponseCurveUser>(IMC);
					if (ModObj.IsValid())
					{
						FString PathX, PathY, PathZ;
						if (UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("response_x_path"), PathX, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("ResponseX"), PathX, Result.Errors, false))
						{
							UserMod->ResponseX = LoadObject<UCurveFloat>(nullptr, *PathX);
						}
						if (UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("response_y_path"), PathY, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("ResponseY"), PathY, Result.Errors, false))
						{
							UserMod->ResponseY = LoadObject<UCurveFloat>(nullptr, *PathY);
						}
						if (UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("response_z_path"), PathZ, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetStringParam(ModObj.ToSharedRef(), TEXT("ResponseZ"), PathZ, Result.Errors, false))
						{
							UserMod->ResponseZ = LoadObject<UCurveFloat>(nullptr, *PathZ);
						}
					}
					NewMod = UserMod;
				}
				else if (ModType.Equals(TEXT("Smooth"), ESearchCase::IgnoreCase))
				{
					NewMod = NewObject<UInputModifierSmooth>(IMC);
				}
				else
				{
					UE_LOG(LogAgentFramework, Warning, TEXT("InputActions: Unknown modifier type '%s', skipping"), *ModType);
				}

				if (IsValid(NewMod))
				{
					Mapping.Modifiers.Add(NewMod);
					AppliedModifiersCount++;
				}
			}
		}
	}

	// Parse triggers array
	const TArray<TSharedPtr<FJsonValue>>* TriggersArray = nullptr;
	if (UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("triggers"), TriggersArray, Result.Errors, false) ||
		UAgentFrameworkActionUtils::TryGetArrayParam(Params, TEXT("Triggers"), TriggersArray, Result.Errors, false))
	{
		if (TriggersArray)
		{
			for (const TSharedPtr<FJsonValue>& TrigVal : *TriggersArray)
			{
				if (!TrigVal.IsValid()) continue;

				FString TrigType;
				TSharedPtr<FJsonObject> TrigObj;

				if (TrigVal->Type == EJson::String)
				{
					TrigType = TrigVal->AsString();
				}
				else if (TrigVal->Type == EJson::Object)
				{
					TrigObj = TrigVal->AsObject();
					if (!UAgentFrameworkActionUtils::TryGetStringParam(TrigObj.ToSharedRef(), TEXT("type"), TrigType, Result.Errors, false) &&
						!UAgentFrameworkActionUtils::TryGetStringParam(TrigObj.ToSharedRef(), TEXT("Type"), TrigType, Result.Errors, false))
					{
						UE_LOG(LogAgentFramework, Warning, TEXT("InputActions: Trigger object missing 'type' or 'Type' field, skipping"));
						continue;
					}
				}

				UInputTrigger* NewTrig = nullptr;

				if (TrigType.Equals(TEXT("Pressed"), ESearchCase::IgnoreCase) ||
					TrigType.Equals(TEXT("Down"), ESearchCase::IgnoreCase))
				{
					NewTrig = NewObject<UInputTriggerPressed>(IMC);
				}
				else if (TrigType.Equals(TEXT("Released"), ESearchCase::IgnoreCase))
				{
					NewTrig = NewObject<UInputTriggerReleased>(IMC);
				}
				else if (TrigType.Equals(TEXT("Hold"), ESearchCase::IgnoreCase))
				{
					UInputTriggerHold* HoldTrig = NewObject<UInputTriggerHold>(IMC);
					if (TrigObj.IsValid())
					{
						double Threshold = 0.5;
						if (TrigObj->TryGetNumberField(TEXT("hold_time_threshold"), Threshold) ||
							TrigObj->TryGetNumberField(TEXT("HoldTimeThreshold"), Threshold) ||
							TrigObj->TryGetNumberField(TEXT("threshold"), Threshold))
						{
							HoldTrig->HoldTimeThreshold = (float)Threshold;
						}

						bool bIsOneShot = true;
						if (UAgentFrameworkActionUtils::TryGetBoolParam(TrigObj.ToSharedRef(), TEXT("is_one_shot"), bIsOneShot, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetBoolParam(TrigObj.ToSharedRef(), TEXT("bIsOneShot"), bIsOneShot, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetBoolParam(TrigObj.ToSharedRef(), TEXT("one_shot"), bIsOneShot, Result.Errors, false))
						{
							HoldTrig->bIsOneShot = bIsOneShot;
						}

						bool bTimeDilation = false;
						if (UAgentFrameworkActionUtils::TryGetBoolParam(TrigObj.ToSharedRef(), TEXT("affected_by_time_dilation"), bTimeDilation, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetBoolParam(TrigObj.ToSharedRef(), TEXT("bAffectedByTimeDilation"), bTimeDilation, Result.Errors, false))
						{
							HoldTrig->bAffectedByTimeDilation = bTimeDilation;
						}
					}
					NewTrig = HoldTrig;
				}
				else if (TrigType.Equals(TEXT("Tap"), ESearchCase::IgnoreCase))
				{
					UInputTriggerTap* TapTrig = NewObject<UInputTriggerTap>(IMC);
					if (TrigObj.IsValid())
					{
						double Threshold = 0.2;
						if (TrigObj->TryGetNumberField(TEXT("tap_release_time_threshold"), Threshold) ||
							TrigObj->TryGetNumberField(TEXT("TapReleaseTimeThreshold"), Threshold) ||
							TrigObj->TryGetNumberField(TEXT("threshold"), Threshold))
						{
							TapTrig->TapReleaseTimeThreshold = (float)Threshold;
						}
					}
					NewTrig = TapTrig;
				}
				else if (TrigType.Equals(TEXT("Pulse"), ESearchCase::IgnoreCase))
				{
					UInputTriggerPulse* PulseTrig = NewObject<UInputTriggerPulse>(IMC);
					if (TrigObj.IsValid())
					{
						double Interval = 1.0;
						if (TrigObj->TryGetNumberField(TEXT("interval"), Interval) ||
							TrigObj->TryGetNumberField(TEXT("Interval"), Interval))
						{
							PulseTrig->Interval = (float)Interval;
						}

						bool bTriggerOnStart = true;
						if (UAgentFrameworkActionUtils::TryGetBoolParam(TrigObj.ToSharedRef(), TEXT("trigger_on_start"), bTriggerOnStart, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetBoolParam(TrigObj.ToSharedRef(), TEXT("bTriggerOnStart"), bTriggerOnStart, Result.Errors, false))
						{
							PulseTrig->bTriggerOnStart = bTriggerOnStart;
						}

						double DoubleLimit = 0.0;
						if (TrigObj->TryGetNumberField(TEXT("trigger_limit"), DoubleLimit) ||
							TrigObj->TryGetNumberField(TEXT("TriggerLimit"), DoubleLimit))
						{
							PulseTrig->TriggerLimit = (int32)DoubleLimit;
						}
					}
					NewTrig = PulseTrig;
				}
				else if (TrigType.Equals(TEXT("ChordAction"), ESearchCase::IgnoreCase) ||
						 TrigType.Equals(TEXT("ChordedAction"), ESearchCase::IgnoreCase) ||
						 TrigType.Equals(TEXT("Chord"), ESearchCase::IgnoreCase))
				{
					UInputTriggerChordAction* ChordTrig = NewObject<UInputTriggerChordAction>(IMC);
					if (TrigObj.IsValid())
					{
						FString ChordPath;
						if (UAgentFrameworkActionUtils::TryGetStringParam(TrigObj.ToSharedRef(), TEXT("chord_action_path"), ChordPath, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetStringParam(TrigObj.ToSharedRef(), TEXT("ChordActionAsset"), ChordPath, Result.Errors, false) ||
							UAgentFrameworkActionUtils::TryGetStringParam(TrigObj.ToSharedRef(), TEXT("ChordAction"), ChordPath, Result.Errors, false))
						{
							UInputAction* ChordIA = LoadObject<UInputAction>(nullptr, *ChordPath);
							if (!IsValid(ChordIA))
							{
								ChordIA = LoadObject<UInputAction>(nullptr, *(ChordPath + TEXT(".") + FPackageName::GetLongPackageAssetName(ChordPath)));
							}
							if (IsValid(ChordIA))
							{
								ChordTrig->ChordAction = ChordIA;
							}
							else
							{
								UE_LOG(LogAgentFramework, Warning, TEXT("InputActions: Could not load ChordAction asset: %s"), *ChordPath);
							}
						}
					}
					NewTrig = ChordTrig;
				}
				else
				{
					UE_LOG(LogAgentFramework, Warning, TEXT("InputActions: Unknown trigger type '%s', skipping"), *TrigType);
				}

				if (IsValid(NewTrig))
				{
					Mapping.Triggers.Add(NewTrig);
					AppliedTriggersCount++;
				}
			}
		}
	}

	// CRITICAL: Ensure at least one trigger exists (default Pressed)
	if (Mapping.Triggers.Num() == 0)
	{
		UInputTriggerPressed* DefaultTrigger = NewObject<UInputTriggerPressed>(IMC);
		if (IsValid(DefaultTrigger))
		{
			Mapping.Triggers.Add(DefaultTrigger);
			AppliedTriggersCount++;
			UE_LOG(LogAgentFramework, Log, TEXT("InputActions: No triggers specified — defaulting to Pressed trigger."));
		}
	}

	// Save modified IMC package
	IMC->MarkPackageDirty();
	UPackage* Package = IMC->GetOutermost();
	if (!IsValid(Package))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to get outermost package for IMC: %s"), *IMCPath));
		return Result;
	}

	FString PackageFileName = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, IMC, *PackageFileName, SaveArgs))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to save modified IMC to disk: %s"), *IMCPath));
		return Result;
	}

	Result.bSuccess = true;
	Result.ResultMessage = FString::Printf(
		TEXT("Configured key mapping %s on %s with %d modifiers and %d triggers"),
		*KeyName, *IMC->GetName(), AppliedModifiersCount, AppliedTriggersCount);
	Result.ModifiedPaths.Add(IMCPath);

	UE_LOG(LogAgentFramework, Log, TEXT("InputActions: Configured key mapping %s on %s (Modifiers: %d, Triggers: %d)"),
		*KeyName, *IMC->GetName(), AppliedModifiersCount, AppliedTriggersCount);
	return Result;
}
