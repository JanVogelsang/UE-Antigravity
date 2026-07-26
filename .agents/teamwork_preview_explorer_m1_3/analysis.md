# Comprehensive Technical Investigation Report: Enhanced Input Action Modifiers & Triggers (`configure_input_mapping_modifiers_triggers`, Spec 5)

## 1. Executive Summary & Context

This investigation report presents a complete technical analysis of the Enhanced Input Action system in `AgentFrameworkActions` for **Milestone 1 (Spec 5)**: `configure_input_mapping_modifiers_triggers`.

The goal of Spec 5 is to eradicate Python fallbacks when configuring key mappings with input modifiers (`UInputModifierNegate`, `UInputModifierSwizzleAxis`, `UInputModifierScalar`, `UInputModifierDeadZone`, `UInputModifierResponseCurve`) and input triggers (`UInputTriggerPressed`, `UInputTriggerReleased`, `UInputTriggerHold`, `UInputTriggerTap`, `UInputTriggerPulse`, `UInputTriggerChordAction`) in Unreal Engine 5's Enhanced Input System (`UInputMappingContext`).

Our analysis confirms that:
1. **Module & Plugin Dependencies**: `EnhancedInput` is already properly declared in `AgentFrameworkActions.Build.cs` (under `PrivateDependencyModuleNames`) and `AgentFramework.uplugin` (under `Plugins`).
2. **Header Includes**: `AgentFrameworkInputActions.cpp` already includes `"InputAction.h"`, `"InputMappingContext.h"`, `"InputModifiers.h"`, and `"InputTriggers.h"`, which expose all standard Unreal Engine 5 modifier and trigger classes.
3. **Class Signatures & Enum Mappings**: All required modifier/trigger properties, enums (`EInputAxisSwizzle`, `EDeadZoneType`), and constructor patterns were inspected directly against Unreal Engine 5.8 headers (`InputModifiers.h` and `InputTriggers.h`).
4. **Implementation Readiness**: The proposed `configure_input_mapping_modifiers_triggers` method can be cleanly integrated into `FAgentFrameworkInputActions` with full parameter validation and robust garbage-collected UObject lifecycle handling.

---

## 2. Module & Header Dependencies Verification

### 2.1 Build.cs Verification
* **Target File**: `AgentFramework/Source/AgentFrameworkActions/AgentFrameworkActions.Build.cs`
* **Observation**:
  ```csharp
  PrivateDependencyModuleNames.AddRange(new string[]
  {
      ...
      // Enhanced Input asset authoring
      "EnhancedInput",
      "InputBlueprintNodes",
      "InputCore",
      ...
  });
  ```
* **Status**: **VERIFIED**. `EnhancedInput`, `InputBlueprintNodes`, and `InputCore` are present. No additional build dependencies are required.

### 2.2 Plugin Descriptor Verification
* **Target File**: `AgentFramework/AgentFramework.uplugin`
* **Observation**:
  ```json
  {
      "Name": "EnhancedInput",
      "Enabled": true
  }
  ```
* **Status**: **VERIFIED**. `EnhancedInput` is enabled as a plugin dependency.

### 2.3 Header Includes Verification
* **Target File**: `AgentFramework/Source/AgentFrameworkActions/Private/Input/AgentFrameworkInputActions.cpp`
* **Observation**:
  Lines 6–9:
  ```cpp
  #include "InputAction.h"
  #include "InputMappingContext.h"
  #include "InputModifiers.h"
  #include "InputTriggers.h"
  ```
* **Status**: **VERIFIED**. The header includes in `AgentFrameworkInputActions.cpp` directly expose `UInputMappingContext`, `UInputAction`, `FEnhancedActionKeyMapping`, base `UInputModifier` and `UInputTrigger`, as well as all concrete subclasses. No extra `#include` directives are necessary.

---

## 3. Deep-Dive Specification: Modifiers

The table below summarizes the exact Unreal Engine C++ header declarations, public properties, enum mappings, and JSON conversion logic for all supported input modifiers.

| Modifier Class | Source Header | Configurable Public Properties | Enum / Types | JSON Property Mapping |
| :--- | :--- | :--- | :--- | :--- |
| **`UInputModifierNegate`** | `InputModifiers.h:253` | `bool bX = true;`<br>`bool bY = true;`<br>`bool bZ = true;` | `bool` | `bX` (bool), `bY` (bool), `bZ` (bool). Defaults to `true` if omitted. |
| **`UInputModifierSwizzleAxis`** | `InputModifiers.h:412` | `EInputAxisSwizzle Order = EInputAxisSwizzle::YXZ;` | `enum class EInputAxisSwizzle : uint8`<br>- `YXZ` (0)<br>- `ZYX` (1)<br>- `XZY` (2)<br>- `YZX` (3)<br>- `ZXY` (4) | `Order` (string). Maps string `"YXZ"`, `"ZYX"`, `"XZY"`, `"YZX"`, `"ZXY"` to enum values. Default: `YXZ`. |
| **`UInputModifierScalar`** | `InputModifiers.h:213` | `FVector Scalar = FVector::OneVector;` | `FVector` | `Scalar` (float or `{X, Y, Z}` object). If single number given, applies uniformly across `FVector(S, S, S)`. |
| **`UInputModifierDeadZone`** | `InputModifiers.h:170` | `float LowerThreshold = 0.2f;`<br>`float UpperThreshold = 1.0f;`<br>`EDeadZoneType Type = EDeadZoneType::Radial;` | `enum class EDeadZoneType : uint8`<br>- `Axial` (0)<br>- `Radial` (1)<br>- `UnscaledRadial` (2) | `LowerThreshold` (float), `UpperThreshold` (float), `Type` (string: `"Axial"`, `"Radial"`, `"UnscaledRadial"`). |
| **`UInputModifierResponseCurveExponential`** | `InputModifiers.h:306` | `FVector CurveExponent = FVector::OneVector;` | `FVector` | `CurveExponent` (float or `{X, Y, Z}` object). Sets exponential curve per axis. |
| **`UInputModifierResponseCurveUser`** | `InputModifiers.h:325` | `TObjectPtr<UCurveFloat> ResponseX;`<br>`TObjectPtr<UCurveFloat> ResponseY;`<br>`TObjectPtr<UCurveFloat> ResponseZ;` | `UCurveFloat*` | `ResponseXAsset`, `ResponseYAsset`, `ResponseZAsset` (string object paths loaded via `LoadObject<UCurveFloat>`). |
| **`UInputModifierSmooth`** | `InputModifiers.h:275` | N/A (Standard multi-frame smoothing) | N/A | `Type: "Smooth"`. Instantiates default `UInputModifierSmooth`. |

---

## 4. Deep-Dive Specification: Triggers

The table below summarizes the exact Unreal Engine C++ header declarations, public properties, defaults, and JSON conversion logic for all supported input triggers.

| Trigger Class | Source Header | Base Class | Configurable Public Properties | JSON Property Mapping |
| :--- | :--- | :--- | :--- | :--- |
| **`UInputTriggerPressed`** | `InputTriggers.h:279` | `UInputTrigger` | `float ActuationThreshold = 0.5f;` | `ActuationThreshold` (float, default 0.5). Fires once when input exceeds threshold. |
| **`UInputTriggerReleased`** | `InputTriggers.h:298` | `UInputTrigger` | `float ActuationThreshold = 0.5f;` | `ActuationThreshold` (float, default 0.5). Fires once when input drops below threshold. |
| **`UInputTriggerHold`** | `InputTriggers.h:318` | `UInputTriggerTimedBase` | `float HoldTimeThreshold = 1.0f;`<br>`bool bIsOneShot = false;`<br>`bool bAffectedByTimeDilation = false;`<br>`float ActuationThreshold = 0.5f;` | `HoldTimeThreshold` (float, default 0.5), `bIsOneShot` (bool, default true), `ActuationThreshold` (float). |
| **`UInputTriggerTap`** | `InputTriggers.h:364` | `UInputTriggerTimedBase` | `float TapReleaseTimeThreshold = 0.2f;`<br>`float ActuationThreshold = 0.5f;` | `TapReleaseTimeThreshold` (float, default 0.2), `ActuationThreshold` (float). |
| **`UInputTriggerPulse`** | `InputTriggers.h:457` | `UInputTriggerTimedBase` | `bool bTriggerOnStart = true;`<br>`float Interval = 1.0f;`<br>`int32 TriggerLimit = 0;`<br>`float ActuationThreshold = 0.5f;` | `Interval` (float, default 1.0), `bTriggerOnStart` (bool, default true), `TriggerLimit` (int, default 0). |
| **`UInputTriggerChordAction`** | `InputTriggers.h:494` | `UInputTrigger` | `TObjectPtr<const UInputAction> ChordAction;`<br>`float ActuationThreshold = 0.5f;` | `ChordActionPath` / `ChordAction` (string object path). Loads `UInputAction` asset via `LoadObject<UInputAction>`. |

---

## 5. Object Lifecycle, Memory Management, & UE Guardrails

### 5.1 Subobject Instantiation
In Unreal Engine 5's Enhanced Input System, key mappings store modifiers and triggers in `TArray<TObjectPtr<UInputModifier>> Modifiers` and `TArray<TObjectPtr<UInputTrigger>> Triggers`.

When constructing modifier or trigger instances in C++:
```cpp
UInputModifierNegate* NegateMod = NewObject<UInputModifierNegate>(IMC);
```
Setting `IMC` (`UInputMappingContext*`) as the outer object ensures:
1. The modifier/trigger subobject belongs to the `UInputMappingContext` asset package.
2. Unreal Engine's Garbage Collection (GC) properly tracks ownership.
3. Serializing `IMC` via `UPackage::SavePackage()` embeds the subobjects into the saved `.uasset` file.

### 5.2 Mandatory Default Trigger Guardrail
Unreal Engine 5 requires that every valid `FEnhancedActionKeyMapping` has at least one active trigger. A mapping with empty triggers can cause warning logs during validation ("There cannot be a null Input Trigger on a key mapping").
- **Guardrail Rule**: If the `Triggers` array in the JSON payload is empty, omitted, or contains only `"default"`, `ExecuteConfigureInputMappingModifiersTriggers` automatically instantiates and attaches a `UInputTriggerPressed` instance.

---

## 6. Proposed C++ Implementation Architecture

### 6.1 Action Registration (`AgentFrameworkInputActions.h`)
Add `ExecuteConfigureInputMappingModifiersTriggers` private declaration:
```cpp
FAgentFrameworkActionResult ExecuteConfigureInputMappingModifiersTriggers(const TSharedRef<FJsonObject>& Params);
```

### 6.2 Tool Registration & Validation (`AgentFrameworkInputActions.cpp`)
1. **`GetSupportedToolNames()`**:
   ```cpp
   return {
       TEXT("create_input_action"),
       TEXT("create_input_mapping_context"),
       TEXT("add_input_mapping"),
       TEXT("configure_input_mapping_modifiers_triggers")
   };
   ```

2. **`ValidateParams()`**:
   ```cpp
   else if (ToolName == TEXT("configure_input_mapping_modifiers_triggers"))
   {
       FString IMCPath, IAPath, KeyName;
       bool bValid = true;
       // Support both ContextAsset/mapping_context_path, InputActionAsset/action_path, Key/key
       bValid &= (Params->HasField(TEXT("ContextAsset")) || Params->HasField(TEXT("mapping_context_path")));
       bValid &= (Params->HasField(TEXT("InputActionAsset")) || Params->HasField(TEXT("action_path")));
       bValid &= (Params->HasField(TEXT("Key")) || Params->HasField(TEXT("key")));
       return bValid;
   }
   ```

3. **`ExecuteConfigureInputMappingModifiersTriggers` Logic Flow**:
   - Extract `ContextAsset`, `InputActionAsset`, and `Key` strings.
   - Load `UInputMappingContext` and `UInputAction`.
   - Resolve `FKey` using `ParseKeyName(KeyName)`.
   - Locate existing `FEnhancedActionKeyMapping*` inside `IMC->GetMappings()`. If not found, call `IMC->MapKey(IA, Key)`.
   - Iterate over `Modifiers` array:
     - Instantiate and configure matching `UInputModifier` (Negate, SwizzleAxis, Scalar, DeadZone, ResponseCurve, Smooth).
     - Push to `Mapping.Modifiers`.
   - Iterate over `Triggers` array:
     - Instantiate and configure matching `UInputTrigger` (Pressed, Released, Hold, Tap, Pulse, ChordAction).
     - Push to `Mapping.Triggers`.
   - Check default trigger guardrail (`Mapping.Triggers.Num() == 0`).
   - Mark `IMC` package dirty and invoke `SaveAssetPackage`.
   - Return `FAgentFrameworkActionResult` with `bSuccess = true`, `AppliedModifiersCount`, and `AppliedTriggersCount`.

---

## 7. Verification Plan & Test Commands

To verify the implementation once coded:
1. **Module Build Verification**:
   ```powershell
   $env:uebp_UATMutexNoWait = '1'
   powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip
   ```
2. **Automated Pytest Execution**:
   ```powershell
   powershell -File .\Tests\run_tests.ps1
   ```
