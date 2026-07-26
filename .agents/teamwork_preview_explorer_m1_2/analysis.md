# Investigation Report: Spec 5 — Enhanced Input Mapping Modifiers & Triggers

**Module**: `AgentFrameworkInputActions` (Enhanced Input System)  
**Target Action Route**: `configure_input_mapping_modifiers_triggers`  
**Schema File**: `AgentFramework/Resources/ToolSchemas/enhanced_input_tools.json`  
**Author**: Explorer 2 (Milestone 1)  
**Date**: 2026-07-26  

---

## 1. Executive Summary

This report presents the complete investigation, specification, and JSON schema updates required to implement `configure_input_mapping_modifiers_triggers` (Spec 5 from `Documentation/PYTHON_FALLBACK_AUDIT.md`).

Currently, `add_input_mapping` creates key bindings in a `UInputMappingContext` (IMC) and accepts simple modifier/trigger type names as strings. However, standard Unreal Engine 5 Enhanced Input workflows require configuring specific properties on modifiers (such as axis swizzle order `Order`, scalar multipliers `ScalarVector`, deadzone thresholds) and triggers (such as `HoldTimeThreshold`, `bIsOneShot`, `TapReleaseTimeThreshold`, and `ChordAction` dependencies).

This specification adds the `configure_input_mapping_modifiers_triggers` tool schema to `enhanced_input_tools.json` and defines the complete interface and C++ implementation blueprint.

---

## 2. Spec 5 Audit Findings (`Documentation/PYTHON_FALLBACK_AUDIT.md`)

* **Subsystem**: Enhanced Input Mapping Modifiers & Triggers
* **Context**: `setup-input` skill & `AgentFrameworkActions/Input`
* **Python Fallback Rationale**:
  ```python
  import unreal
  imc = unreal.load_object(None, '/Game/Input/IMC_Default')
  mapping = imc.add_mapping(input_action, key)
  mod = unreal.EnhancedInputModifierNegate()
  mapping.modifiers.append(mod)
  ```
  `add_input_mapping` cannot configure structured modifier properties (e.g. `Order` for `UInputModifierSwizzleAxis`, `Scalar` vector for `UInputModifierScalar`, `LowerThreshold`/`UpperThreshold` for `UInputModifierDeadZone`) or trigger parameters (`HoldTimeThreshold` for `UInputTriggerHold`, `bIsOneShot` for `UInputTriggerTap`/`Hold`, `ChordAction`).

---

## 3. Schema File & Casing Convention Analysis

### 3.1 File Location Discrepancy
* **Requested Spec Location**: `AgentFramework/Resources/ToolSchemas/input_tools.json`
* **Actual Repository File Path**: `AgentFramework/Resources/ToolSchemas/enhanced_input_tools.json`
* **Category**: `"input"` (`schema_version`: `"1.0"`)

### 3.2 Property Casing Strategy
* Existing tools in `enhanced_input_tools.json` (`create_input_action`, `create_input_mapping_context`, `add_input_mapping`) use **`snake_case`** property names for MCP tool parameters (`mapping_context_path`, `action_path`, `key`, `modifiers`, `triggers`).
* Spec 5 in `PYTHON_FALLBACK_AUDIT.md` specifies **`PascalCase`** property names (`ContextAsset`, `InputActionAsset`, `Key`, `Modifiers`, `Triggers`).
* **Resolution**:
  1. The JSON schema in `enhanced_input_tools.json` defines primary parameters using `snake_case` to align with the rest of `enhanced_input_tools.json`, with descriptions documenting the `PascalCase` aliases.
  2. The C++ parameter validation and execution routines in `FAgentFrameworkInputActions` will check both `snake_case` and `PascalCase` keys via fallback getters (e.g. `mapping_context_path` -> `ContextAsset`).

---

## 4. Exact JSON Schema Updates

### 4.1 Schema Definition for `configure_input_mapping_modifiers_triggers`

```json
{
  "name": "configure_input_mapping_modifiers_triggers",
  "description": "Configure detailed input modifiers (Negate, SwizzleAxis, Scalar, DeadZone, Smooth, ResponseCurve) and triggers (Pressed, Released, Hold, Tap, Pulse, ChordAction) with custom parameters on an existing key binding in an Input Mapping Context.",
  "input_schema": {
    "type": "object",
    "properties": {
      "mapping_context_path": {
        "type": "string",
        "description": "Content path of the Input Mapping Context asset (e.g. /Game/Input/IMC_Default). Alias: ContextAsset"
      },
      "action_path": {
        "type": "string",
        "description": "Content path of the target Input Action asset (e.g. /Game/Input/Actions/IA_Move). Alias: InputActionAsset"
      },
      "key": {
        "type": "string",
        "description": "Hardware key identifier (e.g. 'W', 'Gamepad_LeftStick_Y'). Alias: Key"
      },
      "modifiers": {
        "type": "array",
        "description": "List of detailed input modifier objects to configure",
        "items": {
          "type": "object",
          "properties": {
            "type": {
              "type": "string",
              "description": "Modifier type name. Alias: Type",
              "enum": ["Negate", "SwizzleAxis", "Scalar", "DeadZone", "Smooth", "ResponseCurve"]
            },
            "order": {
              "type": "string",
              "description": "Axis order for SwizzleAxis modifier. Alias: Order",
              "enum": ["YXZ", "ZYX", "XZY"],
              "default": "YXZ"
            },
            "scalar_vector": {
              "type": "object",
              "description": "3D multiplier vector for Scalar modifier. Alias: ScalarVector",
              "properties": {
                "X": { "type": "number" },
                "Y": { "type": "number" },
                "Z": { "type": "number" }
              }
            },
            "lower_threshold": {
              "type": "number",
              "description": "Lower deadzone threshold. Alias: LowerThreshold",
              "default": 0.2
            },
            "upper_threshold": {
              "type": "number",
              "description": "Upper deadzone threshold. Alias: UpperThreshold",
              "default": 0.9
            }
          },
          "required": ["type"]
        }
      },
      "triggers": {
        "type": "array",
        "description": "List of detailed input trigger objects to configure",
        "items": {
          "type": "object",
          "properties": {
            "type": {
              "type": "string",
              "description": "Trigger type name. Alias: Type",
              "enum": ["Pressed", "Released", "Hold", "Tap", "Pulse", "ChordAction"]
            },
            "hold_time_threshold": {
              "type": "number",
              "description": "Required hold duration in seconds for Hold trigger. Alias: HoldTimeThreshold",
              "default": 0.5
            },
            "is_one_shot": {
              "type": "boolean",
              "description": "Whether trigger fires only once per actuation. Alias: bIsOneShot",
              "default": true
            },
            "tap_release_time_threshold": {
              "type": "number",
              "description": "Maximum release time window for Tap trigger. Alias: TapReleaseTimeThreshold",
              "default": 0.2
            },
            "chord_action_path": {
              "type": "string",
              "description": "Content path to required modifier input action for ChordAction trigger. Alias: ChordActionAsset"
            }
          },
          "required": ["type"]
        }
      }
    },
    "required": ["mapping_context_path", "action_path", "key"]
  }
}
```

### 4.2 Full Target File Content (`AgentFramework/Resources/ToolSchemas/enhanced_input_tools.json`)

```json
{
  "schema_version": "1.0",
  "category": "input",
  "tools": [
    {
      "name": "create_input_action",
      "description": "Create a UInputAction data asset (IA_*). This is an Enhanced Input action asset that defines a conceptual input command (e.g., IA_Jump, IA_Move, IA_Look, IA_ToggleCamera). The value_type determines the data shape: Boolean for digital buttons, Axis1D for single-axis (triggers), Axis2D for 2D movement/look, Axis3D for spatial input.",
      "input_schema": {
        "type": "object",
        "properties": {
          "asset_path": {
            "type": "string",
            "description": "Full content path for the new Input Action asset (e.g., /Game/ThirdPerson/Input/Actions/IA_ToggleCamera)"
          },
          "value_type": {
            "type": "string",
            "description": "Input value type: Boolean (digital on/off), Axis1D (float), Axis2D (Vector2D), Axis3D (Vector). Default: Boolean",
            "enum": ["Boolean", "Axis1D", "Axis2D", "Axis3D"]
          },
          "consume_input": {
            "type": "boolean",
            "description": "Whether this action consumes the input, preventing it from reaching other actions. Default: true"
          }
        },
        "required": ["asset_path"]
      }
    },
    {
      "name": "create_input_mapping_context",
      "description": "Create a UInputMappingContext data asset (IMC_*). This is an Enhanced Input mapping context that holds a collection of key-to-action bindings. Mapping contexts are swappable at runtime and have priorities to control which bindings take precedence.",
      "input_schema": {
        "type": "object",
        "properties": {
          "asset_path": {
            "type": "string",
            "description": "Full content path for the new Input Mapping Context asset (e.g., /Game/ThirdPerson/Input/IMC_Default)"
          }
        },
        "required": ["asset_path"]
      }
    },
    {
      "name": "add_input_mapping",
      "description": "Add a hardware key binding to an existing Input Mapping Context, mapping a specific key to an Input Action. Optionally attach input modifiers (Negate, Swizzle, DeadZone, Scalar) and triggers (Pressed, Released, Hold, Tap, Pulse) to the binding.",
      "input_schema": {
        "type": "object",
        "properties": {
          "mapping_context_path": {
            "type": "string",
            "description": "Content path of the Input Mapping Context to modify (e.g., /Game/ThirdPerson/Input/IMC_Default)"
          },
          "action_path": {
            "type": "string",
            "description": "Content path of the Input Action to bind (e.g., /Game/ThirdPerson/Input/Actions/IA_ToggleCamera)"
          },
          "key": {
            "type": "string",
            "description": "Hardware key name (e.g., C, SpaceBar, W, S, A, D, LeftMouseButton, RightMouseButton, Gamepad_FaceButton_Bottom, Mouse2D, Gamepad_LeftThumbstick_2D)"
          },
          "modifiers": {
            "type": "array",
            "description": "Optional list of input modifiers to apply to this mapping",
            "items": {
              "type": "string",
              "enum": ["Negate", "Swizzle", "DeadZone", "Scalar"]
            }
          },
          "triggers": {
            "type": "array",
            "description": "Optional list of input triggers to apply to this mapping. If omitted, the default trigger behavior applies (Pressed).",
            "items": {
              "type": "string",
              "enum": ["Pressed", "Released", "Hold", "Tap", "Pulse"]
            }
          }
        },
        "required": ["mapping_context_path", "action_path", "key"]
      }
    },
    {
      "name": "configure_input_mapping_modifiers_triggers",
      "description": "Configure detailed input modifiers (Negate, SwizzleAxis, Scalar, DeadZone, Smooth, ResponseCurve) and triggers (Pressed, Released, Hold, Tap, Pulse, ChordAction) with custom parameters on an existing key binding in an Input Mapping Context.",
      "input_schema": {
        "type": "object",
        "properties": {
          "mapping_context_path": {
            "type": "string",
            "description": "Content path of the Input Mapping Context asset (e.g. /Game/Input/IMC_Default). Alias: ContextAsset"
          },
          "action_path": {
            "type": "string",
            "description": "Content path of the target Input Action asset (e.g. /Game/Input/Actions/IA_Move). Alias: InputActionAsset"
          },
          "key": {
            "type": "string",
            "description": "Hardware key identifier (e.g. 'W', 'Gamepad_LeftStick_Y'). Alias: Key"
          },
          "modifiers": {
            "type": "array",
            "description": "List of detailed input modifier objects to configure",
            "items": {
              "type": "object",
              "properties": {
                "type": {
                  "type": "string",
                  "description": "Modifier type name. Alias: Type",
                  "enum": ["Negate", "SwizzleAxis", "Scalar", "DeadZone", "Smooth", "ResponseCurve"]
                },
                "order": {
                  "type": "string",
                  "description": "Axis order for SwizzleAxis modifier. Alias: Order",
                  "enum": ["YXZ", "ZYX", "XZY"],
                  "default": "YXZ"
                },
                "scalar_vector": {
                  "type": "object",
                  "description": "3D multiplier vector for Scalar modifier. Alias: ScalarVector",
                  "properties": {
                    "X": { "type": "number" },
                    "Y": { "type": "number" },
                    "Z": { "type": "number" }
                  }
                },
                "lower_threshold": {
                  "type": "number",
                  "description": "Lower deadzone threshold. Alias: LowerThreshold",
                  "default": 0.2
                },
                "upper_threshold": {
                  "type": "number",
                  "description": "Upper deadzone threshold. Alias: UpperThreshold",
                  "default": 0.9
                }
              },
              "required": ["type"]
            }
          },
          "triggers": {
            "type": "array",
            "description": "List of detailed input trigger objects to configure",
            "items": {
              "type": "object",
              "properties": {
                "type": {
                  "type": "string",
                  "description": "Trigger type name. Alias: Type",
                  "enum": ["Pressed", "Released", "Hold", "Tap", "Pulse", "ChordAction"]
                },
                "hold_time_threshold": {
                  "type": "number",
                  "description": "Required hold duration in seconds for Hold trigger. Alias: HoldTimeThreshold",
                  "default": 0.5
                },
                "is_one_shot": {
                  "type": "boolean",
                  "description": "Whether trigger fires only once per actuation. Alias: bIsOneShot",
                  "default": true
                },
                "tap_release_time_threshold": {
                  "type": "number",
                  "description": "Maximum release time window for Tap trigger. Alias: TapReleaseTimeThreshold",
                  "default": 0.2
                },
                "chord_action_path": {
                  "type": "string",
                  "description": "Content path to required modifier input action for ChordAction trigger. Alias: ChordActionAsset"
                }
              },
              "required": ["type"]
            }
          }
        },
        "required": ["mapping_context_path", "action_path", "key"]
      }
    }
  ]
}
```

---

## 5. C++ Implementation Blueprint (`FAgentFrameworkInputActions`)

### 5.1 Class Signature Updates (`AgentFrameworkInputActions.h`)

Add tool registration and execution method:

```cpp
// In FAgentFrameworkInputActions (AgentFrameworkInputActions.h):
virtual TArray<FString> GetSupportedToolNames() const override
{
    return {
        TEXT("create_input_action"),
        TEXT("create_input_mapping_context"),
        TEXT("add_input_mapping"),
        TEXT("configure_input_mapping_modifiers_triggers") // NEW
    };
}

private:
    FAgentFrameworkActionResult ExecuteConfigureInputMappingModifiersTriggers(const TSharedRef<FJsonObject>& Params);
```

### 5.2 Implementation Logic (`AgentFrameworkInputActions.cpp`)

1. **Parameter Resolution**:
   - Extract `IMCPath` from `mapping_context_path` or `ContextAsset`.
   - Extract `IAPath` from `action_path` or `InputActionAsset`.
   - Extract `KeyName` from `key` or `Key`.
2. **Asset Lookup / Mapping Resolution**:
   - Load `UInputMappingContext* IMC` and `UInputAction* IA`.
   - Search existing `FEnhancedActionKeyMapping` entries in `IMC->GetMappings()`.
   - If missing, map key using `IMC->MapKey(IA, Key)` or locate target mapping.
3. **Modifier Application**:
   - For `SwizzleAxis`: Instantiate `UInputModifierSwizzleAxis* SwizzleMod`, set `SwizzleMod->Order = EInputAxisSwizzle::YXZ` (or `ZYX`/`XZY`).
   - For `Scalar`: Instantiate `UInputModifierScalar* ScalarMod`, set `ScalarMod->Scalar = FVector(X, Y, Z)`.
   - For `DeadZone`: Instantiate `UInputModifierDeadZone* DeadZoneMod`, set `DeadZoneMod->LowerThreshold` and `UpperThreshold`.
   - For `Negate`: Instantiate `UInputModifierNegate`.
4. **Trigger Application**:
   - For `Hold`: Instantiate `UInputTriggerHold* HoldTrig`, set `HoldTrig->HoldTimeThreshold` and `HoldTrig->bIsOneShot`.
   - For `Tap`: Instantiate `UInputTriggerTap* TapTrig`, set `TapTrig->TapReleaseTimeThreshold`.
   - For `ChordAction`: Instantiate `UInputTriggerChordAction* ChordTrig`, load `ChordActionAsset` and assign `ChordTrig->ChordAction`.
   - Guard against empty triggers: If zero triggers configured, append default `UInputTriggerPressed`.
5. **Persistence**:
   - Call `IMC->MarkPackageDirty()`.
   - Save package with `UPackage::SavePackage`.

---

## 6. Verification Method

1. **JSON Syntax & Structure Check**:
   Validate `AgentFramework/Resources/ToolSchemas/enhanced_input_tools.json` with a JSON schema validator.
2. **C++ Compilation**:
   Build `AgentFramework` plugin using UAT script:
   `powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip`
3. **Automation / E2E Test Verification**:
   Execute input test suite in `Tests/`:
   `powershell -File .\Tests\run_tests.ps1`
