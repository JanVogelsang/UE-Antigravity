# Investigation Analysis: Enhanced Input Action (`configure_input_mapping_modifiers_triggers` - Spec 5)

## 1. Executive Summary
- **Target Component**: `FAgentFrameworkInputActions` (`AgentFrameworkActions` module)
- **Target Files**:
  - `AgentFramework/Source/AgentFrameworkActions/Public/Input/AgentFrameworkInputActions.h`
  - `AgentFramework/Source/AgentFrameworkActions/Private/Input/AgentFrameworkInputActions.cpp`
- **Objective**: Analyze existing input action tools (`create_input_action`, `create_input_mapping_context`, `add_input_mapping`) and design the full C++ implementation for Spec 5: `configure_input_mapping_modifiers_triggers`.

---

## 2. Examination of Existing Implementation (`FAgentFrameworkInputActions`)

### 2.1 Class Structure & Registration
`FAgentFrameworkInputActions` implements the `IAgentFrameworkActionExecutor` interface from `AgentFrameworkCoreModule`.
- `GetActionName()`: Returns `FName(TEXT("Input"))`.
- `GetSupportedToolNames()`: Returns `{ TEXT("create_input_action"), TEXT("create_input_mapping_context"), TEXT("add_input_mapping") }`.
- `ValidateParams()`: Validates required parameter strings based on `_tool_name`.
- `ExecuteAction()`: Wraps action execution inside `FScopedTransaction` for undo/redo safety. Dispatches by `_tool_name` to internal helper methods. Plays editor success sound on success (`PlaySuccessSound()`), or cancels transaction on failure.

### 2.2 Existing Input Tools Overview
1. **`create_input_action` (`ExecuteCreateInputAction`)**:
   - Creates a `UInputAction` data asset at `asset_path`.
   - Parses `value_type` (`Boolean`, `Axis1D`/`Float`, `Axis2D`/`Vector2D`, `Axis3D`/`Vector`).
   - Parses `consume_input` (bool, default `true`).
   - Uses internal `SaveAssetPackage` helper to dirty the package, notify `FAssetRegistryModule::AssetCreated`, and save via `UPackage::SavePackage`.

2. **`create_input_mapping_context` (`ExecuteCreateInputMappingContext`)**:
   - Creates a `UInputMappingContext` data asset at `asset_path`.
   - Saves package via `SaveAssetPackage`.

3. **`add_input_mapping` (`ExecuteAddInputMapping`)**:
   - Loads target `UInputMappingContext` and `UInputAction`.
   - Parses key string into `FKey`.
   - Calls `IMC->MapKey(IA, Key)` to append key mapping.
   - Parses simple string arrays `modifiers` (e.g., `"Negate"`, `"Swizzle"`) and `triggers` (e.g., `"Pressed"`, `"Hold"`), instantiating parameterless objects via `NewObject<T>()`.
   - Contains a critical UE guardrail: if `Mapping.Triggers.Num() == 0`, automatically appends `UInputTriggerPressed` to avoid engine cooking warnings.

---

## 3. Gap Analysis: Why Spec 5 `configure_input_mapping_modifiers_triggers` is Needed

While `add_input_mapping` can attach basic, unconfigured modifiers and triggers as string tags, it has severe limitations:
1. **No Property Configuration for Modifiers**: Cannot set properties such as `UInputModifierSwizzleAxis::Order` (`YXZ`, `ZYX`, `XZY`), `UInputModifierScalar::Scalar` (`FVector`), or dead zone thresholds (`UInputModifierDeadZone`).
2. **No Property Configuration for Triggers**: Cannot set trigger properties such as `UInputTriggerHold::HoldTimeThreshold` (e.g., 0.5s), `UInputTriggerHold::bIsOneShot`, tap durations, pulse intervals, or chord actions.
3. **Cannot Target Existing Mappings**: `add_input_mapping` always calls `IMC->MapKey(IA, Key)` which appends a mapping rather than modifying or configuring complex modifiers/triggers on an existing mapping in `UInputMappingContext`.

Spec 5 solves this by providing a dedicated, structured C++ action tool `configure_input_mapping_modifiers_triggers`.

---

## 4. Spec 5 Technical Specification & API Mapping

### 4.1 Input Payload Schema
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "ContextAsset": { "type": "string", "description": "Object path of UInputMappingContext asset (or mapping_context_path)" },
    "InputActionAsset": { "type": "string", "description": "Object path of target UInputAction asset (or action_path)" },
    "Key": { "type": "string", "description": "Key identifier (e.g. 'W', 'Gamepad_LeftStick_Y')" },
    "Modifiers": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "Type": { "type": "string", "enum": ["Negate", "SwizzleAxis", "Scalar", "DeadZone", "Smooth", "ResponseCurve"] },
          "Order": { "type": "string", "enum": ["YXZ", "ZYX", "XZY"], "default": "YXZ" },
          "ScalarVector": {
            "type": "object",
            "properties": { "X": {"type": "number"}, "Y": {"type": "number"}, "Z": {"type": "number"} }
          }
        },
        "required": ["Type"]
      }
    },
    "Triggers": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "Type": { "type": "string", "enum": ["Pressed", "Released", "Hold", "Tap", "Pulse", "ChordAction"] },
          "HoldTimeThreshold": { "type": "number", "default": 0.5 },
          "bIsOneShot": { "type": "boolean", "default": true }
        },
        "required": ["Type"]
      }
    }
  },
  "required": ["ContextAsset", "InputActionAsset", "Key"]
}
```

### 4.2 C++ Class & Enums Mapping
| Parameter / Value | Engine C++ Type / Enum | Implementation Action |
|---|---|---|
| `ContextAsset` | `UInputMappingContext*` | `LoadObject<UInputMappingContext>` |
| `InputActionAsset` | `UInputAction*` | `LoadObject<UInputAction>` |
| `Key` | `FKey` | `ParseKeyName(Key)` |
| `Type: "Negate"` | `UInputModifierNegate` | `NewObject<UInputModifierNegate>(IMC)` |
| `Type: "SwizzleAxis"` | `UInputModifierSwizzleAxis` | `NewObject<UInputModifierSwizzleAxis>(IMC)` + set `Order` |
| `Order: "YXZ"` | `ESwizzleAxis::YXZ` | `SwizzleMod->Order = ESwizzleAxis::YXZ` |
| `Order: "ZYX"` | `ESwizzleAxis::ZYX` | `SwizzleMod->Order = ESwizzleAxis::ZYX` |
| `Order: "XZY"` | `ESwizzleAxis::XZY` | `SwizzleMod->Order = ESwizzleAxis::XZY` |
| `Type: "Scalar"` | `UInputModifierScalar` | `NewObject<UInputModifierScalar>(IMC)` + set `Scalar = FVector(X, Y, Z)` |
| `Type: "DeadZone"` | `UInputModifierDeadZone` | `NewObject<UInputModifierDeadZone>(IMC)` |
| `Type: "Pressed"` | `UInputTriggerPressed` | `NewObject<UInputTriggerPressed>(IMC)` |
| `Type: "Released"` | `UInputTriggerReleased` | `NewObject<UInputTriggerReleased>(IMC)` |
| `Type: "Hold"` | `UInputTriggerHold` | `NewObject<UInputTriggerHold>(IMC)` + set `HoldTimeThreshold`, `bIsOneShot` |
| `Type: "Tap"` | `UInputTriggerTap` | `NewObject<UInputTriggerTap>(IMC)` |
| `Type: "Pulse"` | `UInputTriggerPulse` | `NewObject<UInputTriggerPulse>(IMC)` |
| `Type: "ChordAction"` | `UInputTriggerChordAction` | `NewObject<UInputTriggerChordAction>(IMC)` |

---

## 5. Proposed C++ Changes

### 5.1 Updates to `AgentFrameworkInputActions.h`
Add `ExecuteConfigureInputMappingModifiersTriggers` private method and update docstring:

```cpp
// In AgentFrameworkInputActions.h:
class AGENTFRAMEWORKACTIONS_API FAgentFrameworkInputActions : public IAgentFrameworkActionExecutor
{
    // ...
private:
    FAgentFrameworkActionResult ExecuteCreateInputAction(const TSharedRef<FJsonObject>& Params);
    FAgentFrameworkActionResult ExecuteCreateInputMappingContext(const TSharedRef<FJsonObject>& Params);
    FAgentFrameworkActionResult ExecuteAddInputMapping(const TSharedRef<FJsonObject>& Params);
    FAgentFrameworkActionResult ExecuteConfigureInputMappingModifiersTriggers(const TSharedRef<FJsonObject>& Params);

    void PlaySuccessSound();
};
```

### 5.2 Updates to `AgentFrameworkInputActions.cpp`

#### A. Tool Registration & Parameter Validation
```cpp
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
        FString ContextAsset, InputActionAsset, KeyName;
        bool bValid = true;
        // Support both Spec 5 names and existing param aliases
        if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("ContextAsset"), ContextAsset, OutErrors, false))
        {
            bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("mapping_context_path"), ContextAsset, OutErrors, true);
        }
        if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("InputActionAsset"), InputActionAsset, OutErrors, false))
        {
            bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("action_path"), InputActionAsset, OutErrors, true);
        }
        if (!UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("Key"), KeyName, OutErrors, false))
        {
            bValid &= UAgentFrameworkActionUtils::TryGetStringParam(Params, TEXT("key"), KeyName, OutErrors, true);
        }
        return bValid;
    }

    return true;
}
```

#### B. Execution Dispatcher
```cpp
if (ToolName == TEXT("create_input_action"))
    Result = ExecuteCreateInputAction(Params);
else if (ToolName == TEXT("create_input_mapping_context"))
    Result = ExecuteCreateInputMappingContext(Params);
else if (ToolName == TEXT("add_input_mapping"))
    Result = ExecuteAddInputMapping(Params);
else if (ToolName == TEXT("configure_input_mapping_modifiers_triggers"))
    Result = ExecuteConfigureInputMappingModifiersTriggers(Params);
```

#### C. `ExecuteConfigureInputMappingModifiersTriggers` Implementation
```cpp
FAgentFrameworkActionResult FAgentFrameworkInputActions::ExecuteConfigureInputMappingModifiersTriggers(const TSharedRef<FJsonObject>& Params)
{
    FAgentFrameworkActionResult Result;
    Result.bSuccess = false;

    FString ContextAsset, InputActionAsset, KeyName;
    if (!Params->TryGetStringField(TEXT("ContextAsset"), ContextAsset))
    {
        Params->TryGetStringField(TEXT("mapping_context_path"), ContextAsset);
    }
    if (!Params->TryGetStringField(TEXT("InputActionAsset"), InputActionAsset))
    {
        Params->TryGetStringField(TEXT("action_path"), InputActionAsset);
    }
    if (!Params->TryGetStringField(TEXT("Key"), KeyName))
    {
        Params->TryGetStringField(TEXT("key"), KeyName);
    }

    if (ContextAsset.IsEmpty() || InputActionAsset.IsEmpty() || KeyName.IsEmpty())
    {
        Result.Errors.Add(TEXT("Missing required parameters: ContextAsset, InputActionAsset, or Key"));
        return Result;
    }

    // Load assets
    UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *ContextAsset);
    if (!IsValid(IMC))
    {
        IMC = LoadObject<UInputMappingContext>(nullptr, *(ContextAsset + TEXT(".") + FPackageName::GetLongPackageAssetName(ContextAsset)));
        if (!IsValid(IMC))
        {
            Result.Errors.Add(FString::Printf(TEXT("Could not load Input Mapping Context: %s"), *ContextAsset));
            return Result;
        }
    }

    UInputAction* IA = LoadObject<UInputAction>(nullptr, *InputActionAsset);
    if (!IsValid(IA))
    {
        IA = LoadObject<UInputAction>(nullptr, *(InputActionAsset + TEXT(".") + FPackageName::GetLongPackageAssetName(InputActionAsset)));
        if (!IsValid(IA))
        {
            Result.Errors.Add(FString::Printf(TEXT("Could not load Input Action: %s"), *InputActionAsset));
            return Result;
        }
    }

    FKey Key = ParseKeyName(KeyName);
    IMC->Modify();

    // Find existing mapping or add if not present
    FEnhancedActionKeyMapping* TargetMapping = nullptr;
    for (FEnhancedActionKeyMapping& Mapping : IMC->GetMappings())
    {
        if (Mapping.Action == IA && Mapping.Key == Key)
        {
            TargetMapping = &Mapping;
            break;
        }
    }

    if (!TargetMapping)
    {
        TargetMapping = &IMC->MapKey(IA, Key);
    }

    int32 AppliedModifiersCount = 0;
    int32 AppliedTriggersCount = 0;

    // Parse Modifiers Array of Objects
    const TArray<TSharedPtr<FJsonValue>>* ModifiersArray = nullptr;
    if (Params->TryGetArrayField(TEXT("Modifiers"), ModifiersArray) && ModifiersArray)
    {
        for (const TSharedPtr<FJsonValue>& ModVal : *ModifiersArray)
        {
            if (!ModVal.IsValid() || ModVal->Type() != EJson::Object) continue;
            TSharedPtr<FJsonObject> ModObj = ModVal->AsObject();

            FString ModType;
            if (!ModObj->TryGetStringField(TEXT("Type"), ModType)) continue;

            UInputModifier* NewMod = nullptr;
            if (ModType.Equals(TEXT("Negate"), ESearchCase::IgnoreCase))
            {
                NewMod = NewObject<UInputModifierNegate>(IMC);
            }
            else if (ModType.Equals(TEXT("SwizzleAxis"), ESearchCase::IgnoreCase) ||
                     ModType.Equals(TEXT("SwizzleInputAxisValues"), ESearchCase::IgnoreCase) ||
                     ModType.Equals(TEXT("Swizzle"), ESearchCase::IgnoreCase))
            {
                UInputModifierSwizzleAxis* SwizzleMod = NewObject<UInputModifierSwizzleAxis>(IMC);
                FString OrderStr;
                if (ModObj->TryGetStringField(TEXT("Order"), OrderStr))
                {
                    if (OrderStr.Equals(TEXT("ZYX"), ESearchCase::IgnoreCase))
                        SwizzleMod->Order = ESwizzleAxis::ZYX;
                    else if (OrderStr.Equals(TEXT("XZY"), ESearchCase::IgnoreCase))
                        SwizzleMod->Order = ESwizzleAxis::XZY;
                    else
                        SwizzleMod->Order = ESwizzleAxis::YXZ;
                }
                NewMod = SwizzleMod;
            }
            else if (ModType.Equals(TEXT("Scalar"), ESearchCase::IgnoreCase))
            {
                UInputModifierScalar* ScalarMod = NewObject<UInputModifierScalar>(IMC);
                const TSharedPtr<FJsonObject>* VecObj = nullptr;
                if (ModObj->TryGetObjectField(TEXT("ScalarVector"), VecObj) && VecObj && (*VecObj).IsValid())
                {
                    double X = 1.0, Y = 1.0, Z = 1.0;
                    (*VecObj)->TryGetNumberField(TEXT("X"), X);
                    (*VecObj)->TryGetNumberField(TEXT("Y"), Y);
                    (*VecObj)->TryGetNumberField(TEXT("Z"), Z);
                    ScalarMod->Scalar = FVector(X, Y, Z);
                }
                NewMod = ScalarMod;
            }
            else if (ModType.Equals(TEXT("DeadZone"), ESearchCase::IgnoreCase))
            {
                NewMod = NewObject<UInputModifierDeadZone>(IMC);
            }

            if (IsValid(NewMod))
            {
                TargetMapping->Modifiers.Add(NewMod);
                AppliedModifiersCount++;
            }
        }
    }

    // Parse Triggers Array of Objects
    const TArray<TSharedPtr<FJsonValue>>* TriggersArray = nullptr;
    if (Params->TryGetArrayField(TEXT("Triggers"), TriggersArray) && TriggersArray)
    {
        for (const TSharedPtr<FJsonValue>& TrigVal : *TriggersArray)
        {
            if (!TrigVal.IsValid() || TrigVal->Type() != EJson::Object) continue;
            TSharedPtr<FJsonObject> TrigObj = TrigVal->AsObject();

            FString TrigType;
            if (!TrigObj->TryGetStringField(TEXT("Type"), TrigType)) continue;

            UInputTrigger* NewTrig = nullptr;
            if (TrigType.Equals(TEXT("Pressed"), ESearchCase::IgnoreCase))
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
                double HoldTime = 0.5;
                if (TrigObj->TryGetNumberField(TEXT("HoldTimeThreshold"), HoldTime))
                {
                    HoldTrig->HoldTimeThreshold = static_cast<float>(HoldTime);
                }
                bool bOneShot = true;
                if (TrigObj->TryGetBoolField(TEXT("bIsOneShot"), bOneShot))
                {
                    HoldTrig->bIsOneShot = bOneShot;
                }
                NewTrig = HoldTrig;
            }
            else if (TrigType.Equals(TEXT("Tap"), ESearchCase::IgnoreCase))
            {
                NewTrig = NewObject<UInputTriggerTap>(IMC);
            }
            else if (TrigType.Equals(TEXT("Pulse"), ESearchCase::IgnoreCase))
            {
                NewTrig = NewObject<UInputTriggerPulse>(IMC);
            }
            else if (TrigType.Equals(TEXT("ChordAction"), ESearchCase::IgnoreCase))
            {
                NewTrig = NewObject<UInputTriggerChordAction>(IMC);
            }

            if (IsValid(NewTrig))
            {
                TargetMapping->Triggers.Add(NewTrig);
                AppliedTriggersCount++;
            }
        }
    }

    // Default trigger safety
    if (TargetMapping->Triggers.Num() == 0)
    {
        UInputTriggerPressed* DefaultTrigger = NewObject<UInputTriggerPressed>(IMC);
        if (IsValid(DefaultTrigger))
        {
            TargetMapping->Triggers.Add(DefaultTrigger);
            AppliedTriggersCount++;
        }
    }

    // Save package
    IMC->MarkPackageDirty();
    UPackage* Package = IMC->GetOutermost();
    if (!IsValid(Package))
    {
        Result.Errors.Add(FString::Printf(TEXT("Failed to get outermost package for IMC: %s"), *ContextAsset));
        return Result;
    }

    FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    if (!UPackage::SavePackage(Package, IMC, *PackageFileName, SaveArgs))
    {
        Result.Errors.Add(FString::Printf(TEXT("Failed to save modified IMC to disk: %s"), *ContextAsset));
        return Result;
    }

    Result.bSuccess = true;
    Result.ResultMessage = FString::Printf(
        TEXT("Successfully configured %d modifiers and %d triggers for key '%s' in IMC '%s'"),
        AppliedModifiersCount, AppliedTriggersCount, *KeyName, *ContextAsset);
    Result.ModifiedPaths.Add(ContextAsset);

    UE_LOG(LogAgentFramework, Log, TEXT("InputActions: Configured modifiers/triggers for key '%s' in IMC '%s'"), *KeyName, *ContextAsset);
    return Result;
}
```

---

## 6. Verification and Risk Assessment

### 6.1 Verification Method
1. **Compile Verification**: Execute plugin build via `build_plugin.ps1 -NoZip` to verify UBT compilation of `AgentFrameworkActions`.
2. **Automated Integration Tests**: Run `python -m pytest` or `Tests/run_tests.ps1` with a test payload targeting `configure_input_mapping_modifiers_triggers`.
3. **Editor Asset Inspection**: Load `IMC_Default` asset in target project `AgentFrameworkTest`, inspect `Mappings` array, and verify attached `UInputModifierSwizzleAxis` (Order=YXZ) and `UInputTriggerHold` (HoldTimeThreshold=0.5) objects.

### 6.2 Risks & Mitigation
- **Outer Object Lifetime**: Passing `IMC` as outer in `NewObject<UInputModifier>(IMC)` ensures proper garbage collection tracking and outer package ownership.
- **Null Trigger Cook Crash**: Guaranteed mitigated by enforcing `TargetMapping->Triggers.Num() > 0` check before saving.
