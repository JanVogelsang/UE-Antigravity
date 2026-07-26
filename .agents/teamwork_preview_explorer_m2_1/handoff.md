# Handoff Report: Explorer 1 - Milestone 2: Niagara Action (`set_niagara_parameter`, Spec 6)

## 1. Observation

- **Target Action Class & Source Paths**:
  - Header: `AgentFramework/Source/AgentFrameworkActions/Public/Niagara/AgentFrameworkNiagaraActions.h` (Lines 1-40)
  - Implementation: `AgentFramework/Source/AgentFrameworkActions/Private/Niagara/AgentFrameworkNiagaraActions.cpp` (Lines 1-853)
  - Tool Schema Config: `AgentFramework/Resources/ToolSchemas/niagara_tools.json` (Lines 1-144)
  - Registration: `AgentFramework/Source/AgentFrameworkActions/Private/AgentFrameworkHttpServer.cpp` (Line 99)
  - Specification Doc: `Documentation/PYTHON_FALLBACK_AUDIT.md` (Lines 591-653)

- **Existing Tool Architecture**:
  - `FAgentFrameworkNiagaraActions` implements `IAgentFrameworkActionExecutor`.
  - `GetActionName()` returns `FName(TEXT("Niagara"))` (`AgentFrameworkNiagaraActions.cpp`, Line 37).
  - `GetSupportedToolNames()` returns an array of 6 tool names (`create_niagara_system`, `add_niagara_emitter`, `add_niagara_module`, `set_niagara_module_pin`, `compile_niagara_system`, `capture_niagara_system_isolated`) (`AgentFrameworkNiagaraActions.cpp`, Lines 39-49).
  - `ValidateParams()` validates incoming JSON parameter objects using `UAgentFrameworkActionUtils` helper functions (`AgentFrameworkNiagaraActions.cpp`, Lines 51-107).
  - `ExecuteAction()` manages transaction scopes via `FScopedTransaction`, routes `_tool_name` to specific private execution handlers, triggers `PlaySuccessSound()` on success, and handles transaction cancellation on error (`AgentFrameworkNiagaraActions.cpp`, Lines 109-147).
  - Module Registration: Instantiated and registered in `FAgentFrameworkHttpServer::RegisterAllExecutors` via `InRouter->RegisterExecutor(MakeShared<FAgentFrameworkNiagaraActions>());` (`AgentFrameworkHttpServer.cpp`, Line 99).

- **Spec 6 Requirements (`set_niagara_parameter`)**:
  - Tool Name: `set_niagara_parameter`
  - Purpose: Provide native C++ execution for setting Niagara User/System/Emitter parameters (`User.SpawnRate`, `User.MyColor`, etc.) and dynamic curves (`UCurveFloat`, `UCurveLinearColor`), eliminating Python fallback reliance.
  - JSON Schema Inputs (`PYTHON_FALLBACK_AUDIT.md`, Lines 607-631):
    - `SystemAsset` (or `system_path` / `asset_path`): string (object path of `UNiagaraSystem`)
    - `ParameterScope`: string, enum `["User", "System", "Emitter"]`, default `"User"`
    - `ParameterName`: string (e.g. `"SpawnRate"`, `"PrimaryColor"`)
    - `DataType`: string, enum `["Float", "Vector2", "Vector3", "LinearColor", "Bool", "Int32", "CurveFloat", "CurveLinearColor"]`, default `"Float"`
    - `Value`: constant scalar, vector, or color payload
    - `CurveKeys`: array of keyframe objects `{ "Time": number, "Value": number }` (or vector/color payload)
  - JSON Schema Return (`PYTHON_FALLBACK_AUDIT.md`, Lines 633-647):
    - `{ "bSuccess": bool, "SystemAsset": string, "ParameterName": string, "BoundDataType": string, "ResultMessage": string, "Errors": array<string> }`

---

## 2. Logic Chain

1. **Routing and Tool Exposure Integration**:
   - `FAgentFrameworkNiagaraActions::GetSupportedToolNames()` must include `TEXT("set_niagara_parameter")` so the router and HTTP server acknowledge the tool.
   - `AgentFramework/Resources/ToolSchemas/niagara_tools.json` must be updated with the JSON Schema definition for `set_niagara_parameter` so bridge and LLM tool definitions stay synchronized.

2. **Validation and Command Routing**:
   - In `FAgentFrameworkNiagaraActions::ValidateParams()`, a dedicated validation block for `set_niagara_parameter` must check for `SystemAsset` (or `system_path`/`asset_path`), `ParameterName`, and `DataType`.
   - In `FAgentFrameworkNiagaraActions::ExecuteAction()`, a dispatch condition `else if (ToolName == TEXT("set_niagara_parameter")) Result = ExecuteSetParameter(Params, Result);` routes execution to the new private method.

3. **C++ Execution Strategy for Parameter Store Modification**:
   - **Asset Loading**: Load `UNiagaraSystem` via `LoadObject<UNiagaraSystem>(nullptr, *SystemPath)`. Validate pointer and return error if missing.
   - **Parameter Store Access**: Obtain parameter store reference `FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();` for `User` scope parameters.
   - **Parameter Naming**: Construct the full variable name based on `ParameterScope`. If scope is `"User"` and `ParameterName` does not already begin with `"User."`, format as `User.<ParameterName>`.
   - **Type Resolution**: Map `DataType` string to standard `FNiagaraTypeDefinition`:
     - `"Float"` -> `FNiagaraTypeDefinition::GetFloatDef()`
     - `"Vector2"` -> `FNiagaraTypeDefinition::GetVec2Def()`
     - `"Vector3"` -> `FNiagaraTypeDefinition::GetVec3Def()`
     - `"LinearColor"` -> `FNiagaraTypeDefinition::GetColorDef()`
     - `"Bool"` -> `FNiagaraTypeDefinition::GetBoolDef()`
     - `"Int32"` -> `FNiagaraTypeDefinition::GetIntDef()`
     - `"CurveFloat"` -> `FNiagaraTypeDefinition(UCurveFloat::StaticClass())`
     - `"CurveLinearColor"` -> `FNiagaraTypeDefinition(UCurveLinearColor::StaticClass())`
   - **Parameter Construction & Mutation**:
     - Construct `FNiagaraVariable Variable(TypeDef, FName(*FullParamName));`.
     - For primitive types (`Float`, `Vector2`, `Vector3`, `LinearColor`, `Bool`, `Int32`), extract value from JSON and set data via `UserStore.SetParameterData(...)` or `UserStore.SetParameterValue(...)`.
     - For curve types (`CurveFloat`, `CurveLinearColor`), instantiate a subobject `UCurveFloat` or `UCurveLinearColor` on `System`, populate keyframes into the inner `FRichCurve` objects, and pass the curve `UObject*` pointer into the parameter store variable.
   - **Asset Persistence and Compilation**:
     - Call `System->Modify()` and `Package->MarkPackageDirty()`.
     - Request system compilation via `System->RequestCompile(false)` or `WaitAndReportCompile(System, Result)`.
     - Add `SystemPath` to `Result.ModifiedAssets`.

---

## 3. Caveats

- **Editor-Only Context**: `UNiagaraSystem` graph and parameter store modifications must be wrapped in `#if WITH_EDITOR` guards, matching the pattern in existing tools like `ExecuteAddEmitter` and `ExecuteAddModule`.
- **Curve Object Ownership**: `UCurveFloat` / `UCurveLinearColor` instances assigned to parameter stores must be created with `System` as `Outer` to ensure proper garbage collection tracing and package serialization.
- **Scope Naming Normalization**: `ParameterScope` defaults to `"User"`. Parameter name resolution must check whether the caller passed `"User.MyVar"` or `"MyVar"` to prevent accidental double-prefixing (`"User.User.MyVar"`).

---

## 4. Conclusion

The existing Niagara tool executor (`FAgentFrameworkNiagaraActions`) provides a clean, modular structure for adding `set_niagara_parameter`. Adding Spec 6 requires:
1. Extending `AgentFrameworkNiagaraActions.h` with `ExecuteSetParameter(...)`.
2. Updating `AgentFrameworkNiagaraActions.cpp` (`GetSupportedToolNames`, `ValidateParams`, `ExecuteAction`, `ExecuteSetParameter`).
3. Adding schema entry to `niagara_tools.json`.

All required Unreal Engine Niagara subsystem dependencies (`Niagara`, `NiagaraCore`, `NiagaraEditor`) are already included in `AgentFrameworkActions.Build.cs`.

---

## 5. Verification Method

- **Files to Inspect**:
  - `AgentFramework/Source/AgentFrameworkActions/Public/Niagara/AgentFrameworkNiagaraActions.h`
  - `AgentFramework/Source/AgentFrameworkActions/Private/Niagara/AgentFrameworkNiagaraActions.cpp`
  - `AgentFramework/Resources/ToolSchemas/niagara_tools.json`
- **Build Verification Command**:
  ```powershell
  $env:uebp_UATMutexNoWait = '1'
  powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip
  ```
- **Automated Test Verification**:
  ```powershell
  powershell -File .\Tests\run_tests.ps1
  ```
