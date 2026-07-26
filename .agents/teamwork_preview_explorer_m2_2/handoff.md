# Handoff Report: Milestone 2 - Niagara Action (`set_niagara_parameter`, Spec 6) Schema Design

## 1. Observation

### Existing Tool Schemas
File: `AgentFramework/Resources/ToolSchemas/niagara_tools.json` (144 lines)
- Currently contains 6 tools: `create_niagara_system`, `add_niagara_emitter`, `add_niagara_module`, `set_niagara_module_pin`, `compile_niagara_system`, and `capture_niagara_system_isolated`.
- Existing tools use `snake_case` property names (`system_path`, `emitter_name`, `phase`, `module_type`, `pin_name`, `value`, `duration_seconds`, `max_dimension`).

### Specification 6 Details
File: `Documentation/PYTHON_FALLBACK_AUDIT.md` (lines 591-647)
- **Subsystem**: `Niagara System User Parameters & Curves`
- **Context**: `niagara-authoring` skill & `AgentFrameworkActions/Niagara`
- **Reason Native Actions Are Insufficient**: `set_niagara_module_pin` only mutates module-level inputs. Setting System/Emitter-level User parameters (`User.MyColor`, `User.SpawnRate`) or dynamic curve parameters (`UCurveFloat`, `UCurveLinearColor`) requires native parameter store access via `FNiagaraUserRedirectionParameterStore`.
- **Proposed Action Route Name**: `set_niagara_parameter`
- **Action Module**: `AgentFrameworkNiagaraActions`
- **Parameters Specified in Spec 6**:
  - `SystemAsset` (string, object path of UNiagaraSystem asset)
  - `ParameterScope` (string, enum `["User", "System", "Emitter"]`, default `"User"`)
  - `ParameterName` (string, e.g. `'SpawnRate'`, `'PrimaryColor'`)
  - `DataType` (string, enum `["Float", "Vector2", "Vector3", "LinearColor", "Bool", "Int32", "CurveFloat", "CurveLinearColor"]`, default `"Float"`)
  - `Value` (constant scalar, vector, color, bool, int payload)
  - `CurveKeys` (array of objects with `Time` and `Value` numbers)

### C++ Action Handler
File: `AgentFramework/Source/AgentFrameworkActions/Private/Niagara/AgentFrameworkNiagaraActions.cpp`
- `GetSupportedToolNames()` lists active Niagara tools.
- `ValidateParams()` and `ExecuteAction()` route requests based on `_tool_name`.

---

## 2. Logic Chain

1. **Requirement Alignment**: Spec 6 defines `set_niagara_parameter` with `PascalCase` parameters (`SystemAsset`, `ParameterScope`, `ParameterName`, `DataType`, `Value`, `CurveKeys`), while existing `niagara_tools.json` definitions use `snake_case` (`system_path`, `parameter_scope`, `parameter_name`, `data_type`, `value`, `curve_keys`).
2. **Dual-Alias Strategy**: To ensure compatibility with both Python fallback callers using `PascalCase` from Spec 6 and standard LLM / MCP tool callers using `snake_case`, the tool input schema must define both versions of each parameter as property aliases.
3. **Requirement Validation via `anyOf`**:
   - `snake_case` caller minimal required fields: `["system_path", "parameter_name"]`
   - `PascalCase` caller minimal required fields: `["SystemAsset", "ParameterName"]`
   - Using JSON Schema `anyOf`, either convention will pass validation cleanly.
4. **Schema Addition Design**:
   Below is the complete JSON tool schema object to be appended to the `tools` array in `niagara_tools.json`:

```json
{
  "name": "set_niagara_parameter",
  "description": "Set a System, Emitter, or User parameter value or dynamic curve on a UNiagaraSystem asset. Supports both PascalCase and snake_case parameter aliases.",
  "input_schema": {
    "type": "object",
    "properties": {
      "system_path": {
        "type": "string",
        "description": "Content path of the target UNiagaraSystem asset, e.g. /Game/Effects/NS_Explosion. Alias for SystemAsset."
      },
      "SystemAsset": {
        "type": "string",
        "description": "Object path of UNiagaraSystem asset. Alias for system_path."
      },
      "parameter_scope": {
        "type": "string",
        "enum": ["User", "System", "Emitter"],
        "default": "User",
        "description": "Parameter store scope ('User', 'System', or 'Emitter'). Default: 'User'. Alias for ParameterScope."
      },
      "ParameterScope": {
        "type": "string",
        "enum": ["User", "System", "Emitter"],
        "default": "User",
        "description": "Parameter store scope ('User', 'System', or 'Emitter'). Default: 'User'. Alias for parameter_scope."
      },
      "parameter_name": {
        "type": "string",
        "description": "Name of parameter to set (e.g. 'SpawnRate', 'PrimaryColor', 'User.SpawnRate'). Alias for ParameterName."
      },
      "ParameterName": {
        "type": "string",
        "description": "Name of parameter to set (e.g. 'SpawnRate', 'PrimaryColor'). Alias for parameter_name."
      },
      "data_type": {
        "type": "string",
        "enum": [
          "Float",
          "Vector2",
          "Vector3",
          "LinearColor",
          "Bool",
          "Int32",
          "CurveFloat",
          "CurveLinearColor"
        ],
        "default": "Float",
        "description": "Niagara parameter data type. Default: 'Float'. Alias for DataType."
      },
      "DataType": {
        "type": "string",
        "enum": [
          "Float",
          "Vector2",
          "Vector3",
          "LinearColor",
          "Bool",
          "Int32",
          "CurveFloat",
          "CurveLinearColor"
        ],
        "default": "Float",
        "description": "Niagara parameter data type. Default: 'Float'. Alias for data_type."
      },
      "value": {
        "description": "Constant scalar, vector, color, bool, or int value payload (e.g. '500.0', '1.0, 0.0, 0.0', true). Alias for Value."
      },
      "Value": {
        "description": "Constant scalar, vector, color, bool, or int value payload. Alias for value."
      },
      "curve_keys": {
        "type": "array",
        "description": "Array of curve keyframes for CurveFloat or CurveLinearColor data types. Alias for CurveKeys.",
        "items": {
          "type": "object",
          "properties": {
            "Time": {
              "type": "number",
              "description": "Keyframe time index. Alias for time."
            },
            "time": {
              "type": "number",
              "description": "Keyframe time index. Alias for Time."
            },
            "Value": {
              "type": "number",
              "description": "Keyframe float value. Alias for value."
            },
            "value": {
              "type": "number",
              "description": "Keyframe float value. Alias for Value."
            }
          }
        }
      },
      "CurveKeys": {
        "type": "array",
        "description": "Array of curve keyframes for CurveFloat or CurveLinearColor data types. Alias for curve_keys.",
        "items": {
          "type": "object",
          "properties": {
            "Time": {
              "type": "number",
              "description": "Keyframe time index. Alias for time."
            },
            "time": {
              "type": "number",
              "description": "Keyframe time index. Alias for Time."
            },
            "Value": {
              "type": "number",
              "description": "Keyframe float value. Alias for value."
            },
            "value": {
              "type": "number",
              "description": "Keyframe float value. Alias for Value."
            }
          }
        }
      }
    },
    "anyOf": [
      { "required": ["system_path", "parameter_name"] },
      { "required": ["SystemAsset", "ParameterName"] }
    ]
  }
}
```

---

## 3. Caveats

- **C++ Parameter Lookup Requirement**: When implementing the C++ handler (`FAgentFrameworkNiagaraActions::ExecuteSetNiagaraParameter`), parameter extraction logic must check for snake_case fields first and fall back to PascalCase fields (e.g. `Params->HasField("system_path") ? ... : ...`).
- **Value Payload Flexibility**: The `value` / `Value` property allows string, numeric, boolean, or structured payloads. C++ parsing should handle string conversion appropriately for vectors/colors (e.g., `'1.0, 0.0, 0.0'` or `'(R=1.0,G=0.0,B=0.0)'`).

---

## 4. Conclusion

The exact JSON schema for `set_niagara_parameter` has been designed with complete support for both `PascalCase` (`SystemAsset`, `ParameterScope`, `ParameterName`, `DataType`, `Value`, `CurveKeys`) and `snake_case` (`system_path`, `parameter_scope`, `parameter_name`, `data_type`, `value`, `curve_keys`) aliases. Appending this schema addition to `niagara_tools.json` satisfies Milestone 2 Spec 6 requirements.

---

## 5. Verification Method

1. **JSON Syntax Verification**: Validate `AgentFramework/Resources/ToolSchemas/niagara_tools.json` with a JSON linter to ensure syntax validity after adding the schema entry.
2. **Schema Verification**: Verify that both of the following payloads match the schema:
   - Snake case payload:
     `{"system_path": "/Game/VFX/NS_Explosion", "parameter_scope": "User", "parameter_name": "SpawnRate", "data_type": "Float", "value": "500.0"}`
   - Pascal case payload:
     `{"SystemAsset": "/Game/VFX/NS_Explosion", "ParameterScope": "User", "ParameterName": "SpawnRate", "DataType": "Float", "Value": 500.0}`
