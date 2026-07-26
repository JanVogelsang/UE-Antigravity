## 2026-07-26T13:34:28Z
You are the Worker for Milestone 2: Niagara Action (`set_niagara_parameter`, Spec 6).
Working directory: c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\teamwork_preview_worker_m2\

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Task Objectives:
1. Implement `set_niagara_parameter` (Spec 6) in `FAgentFrameworkNiagaraActions` (`AgentFramework/Source/AgentFrameworkActions/Public/Niagara/AgentFrameworkNiagaraActions.h` and `AgentFramework/Source/AgentFrameworkActions/Private/Niagara/AgentFrameworkNiagaraActions.cpp`).
2. Update `AgentFramework/Resources/ToolSchemas/niagara_tools.json` to register the schema for `set_niagara_parameter`.
3. Support both `PascalCase` (`SystemAsset`, `ParameterScope`, `ParameterName`, `DataType`, `Value`, `CurveKeys`) and `snake_case` (`system_path`, `parameter_scope`, `parameter_name`, `data_type`, `value`, `curve_keys`) parameter aliases.
4. Include required headers in `AgentFrameworkNiagaraActions.cpp`:
   - `#include "NiagaraUserRedirectionParameterStore.h"`
   - `#include "NiagaraTypes.h"`
   - `#include "Curves/CurveFloat.h"`
   - `#include "Curves/CurveLinearColor.h"`
5. Implement parameter setting logic:
   - Load `UNiagaraSystem` asset.
   - Access parameter store via `FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();` (or system override store).
   - Format parameter variable name as `Scope.ParamName` (e.g. `User.SpawnRate`).
   - Support data types: `Float`, `Vector2`, `Vector3`, `LinearColor`, `Bool`, `Int32`, `CurveFloat`, `CurveLinearColor`.
   - For float, vector, color, bool, int32: set parameter data in store using appropriate `FNiagaraVariable` and `FNiagaraTypeDefinition`.
   - For curves (`CurveFloat`, `CurveLinearColor`): construct transient curve objects parented to system, populate keyframes using `FRichCurve::AddKey`, and bind to store via `SetUObject`.
   - Call `System->RequestCompile(false)`, mark package dirty, and save package via `UPackage::SavePackage`.
6. Compile plugin using `powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip` from repository root.
7. Document implementation details and build results in `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\teamwork_preview_worker_m2\handoff.md`.
8. Send completion message to parent (ID: c0ae05a7-3e22-4807-b941-1f254eb25f71).

Refer to Explorer handoff reports:
- `.agents/teamwork_preview_explorer_m2_2/handoff.md`
- `.agents/teamwork_preview_explorer_m2_3/handoff.md`
