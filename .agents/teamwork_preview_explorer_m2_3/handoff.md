# Handoff Report: Niagara Action Audit (`set_niagara_parameter`, Spec 6)

**Agent Role**: Explorer 3 for Milestone 2  
**Working Directory**: `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\teamwork_preview_explorer_m2_3\`  
**Target Milestone**: Milestone 2 - Niagara Action (`set_niagara_parameter`, Spec 6)  
**Date**: 2026-07-26  

---

## 1. Observation

### Codebase Inspection & Module Dependencies
- **Build Configuration**: Inspected `AgentFramework/Source/AgentFrameworkActions/AgentFrameworkActions.Build.cs` (lines 87–89). `PrivateDependencyModuleNames` already contains `"Niagara"`, `"NiagaraCore"`, and `"NiagaraEditor"`. The `Engine` module is included in `PublicDependencyModuleNames` (line 15).
- **Header Files**:
  - `NiagaraSystem.h` is already included in `AgentFrameworkNiagaraActions.cpp` (line 8).
  - The following headers are **missing** and must be added to `AgentFrameworkNiagaraActions.cpp`:
    - `#include "NiagaraUserRedirectionParameterStore.h"`
    - `#include "NiagaraTypes.h"`
    - `#include "Curves/CurveFloat.h"`
    - `#include "Curves/CurveLinearColor.h"`
- **Existing Niagara Executor Structure**:
  - Header: `AgentFramework/Source/AgentFrameworkActions/Public/Niagara/AgentFrameworkNiagaraActions.h`
  - Source: `AgentFramework/Source/AgentFrameworkActions/Private/Niagara/AgentFrameworkNiagaraActions.cpp`
  - Schema: `AgentFramework/Resources/ToolSchemas/niagara_tools.json` (currently defines 6 tools, `set_niagara_parameter` is missing).

### Niagara C++ Engine API Inventory
1. **System & Parameter Store**:
   - Class: `UNiagaraSystem` (`#include "NiagaraSystem.h"`)
   - Method: `FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();`
2. **Variable & Type Definitions**:
   - Variable Class: `FNiagaraVariable` (`#include "NiagaraTypes.h"`)
   - Variable Construction: `FNiagaraVariable Var(TypeDef, FName(*FullParamName));`
   - Naming Convention: Scope prefix (`"User"`, `"System"`, `"Emitter"`). E.g. `"User.SpawnRate"`.
   - Data Type Mapping:
     - `Float` -> `FNiagaraTypeDefinition::GetFloatDef()`
     - `Vector2` -> `FNiagaraTypeDefinition::GetVec2Def()`
     - `Vector3` -> `FNiagaraTypeDefinition::GetVec3Def()`
     - `LinearColor` -> `FNiagaraTypeDefinition::GetColorDef()`
     - `Bool` -> `FNiagaraTypeDefinition::GetBoolDef()`
     - `Int32` -> `FNiagaraTypeDefinition::GetIntDef()`
     - `CurveFloat` -> `FNiagaraTypeDefinition(UCurveFloat::StaticClass())`
     - `CurveLinearColor` -> `FNiagaraTypeDefinition(UCurveLinearColor::StaticClass())`
3. **Parameter Store Mutation**:
   - Parameter Registration: `if (UserStore.IndexOf(Var) == INDEX_NONE) { UserStore.AddParameter(Var, true); }`
   - Primitive Data Assignment: `UserStore.SetParameterData((const uint8*)&Data, Var);`
   - UObject / Curve Assignment: `UserStore.SetUObject(CurveObject, Var);`
4. **Curve Objects (`UCurveFloat`, `UCurveLinearColor`)**:
   - `UCurveFloat`: Outer system allocation `NewObject<UCurveFloat>(System, NAME_None, RF_Transactional);`. Manipulates `FloatCurve` (`FRichCurve`), calling `FloatCurve.Reset()` and `FloatCurve.AddKey(Time, Value)`.
   - `UCurveLinearColor`: Outer system allocation `NewObject<UCurveLinearColor>(System, NAME_None, RF_Transactional);`. Manipulates `FloatCurves[4]` (`FRichCurve` array for R, G, B, A), calling `FloatCurves[i].Reset()` and `FloatCurves[i].AddKey(Time, ColorChannel)`.

---

## 2. Logic Chain

1. **Module & Header Audit**:
   - Observed that `AgentFrameworkActions.Build.cs` already links against `"Niagara"`, `"NiagaraCore"`, and `"NiagaraEditor"`.
   - Observed that curve classes (`UCurveFloat`, `UCurveLinearColor`) reside in the `Engine` module which is already linked.
   - Therefore, zero modifications to `AgentFrameworkActions.Build.cs` are required.
   - Adding missing headers (`NiagaraUserRedirectionParameterStore.h`, `NiagaraTypes.h`, `Curves/CurveFloat.h`, `Curves/CurveLinearColor.h`) to `AgentFrameworkNiagaraActions.cpp` will unlock full C++ Niagara parameter manipulation.

2. **Parameter Store Integration**:
   - `UNiagaraSystem::GetExposedParameters()` yields the `FNiagaraUserRedirectionParameterStore`.
   - Searching or adding `FNiagaraVariable` with `UserStore.IndexOf(Var)` guarantees parameters exist before writing values.
   - Setting primitive values via `SetParameterData` or UObjects via `SetUObject` updates the parameter store cleanly in memory.

3. **Curve Construction**:
   - Creating transient `UCurveFloat` or `UCurveLinearColor` instances parented to `UNiagaraSystem` prevents garbage collection of curve objects.
   - `FRichCurve::AddKey(float Time, float Value)` inserts keyframes cleanly into the curve object.
   - Binding the generated curve object to the parameter store via `SetUObject` allows Niagara modules to evaluate dynamic user curves.

4. **Recompilation & Pipeline Integration**:
   - Marking system dirty (`System->Modify()`, `Package->MarkPackageDirty()`) and triggering `WaitAndReportCompile(System, Result)` ensures changes are validated and compiled immediately.

---

## 3. Caveats

- **Scope Prefix**: Parameter names must be properly scoped. If `parameter_scope` is provided as `"User"` and `parameter_name` is `"SpawnRate"`, the full variable name constructed must be `"User.SpawnRate"`. If the user passes `"User.SpawnRate"` in `parameter_name`, duplicate scope prefixes must be avoided.
- **Curve Lifetime**: Curve objects created via `NewObject<UCurveFloat>` or `NewObject<UCurveLinearColor>` must use `UNiagaraSystem` as their `Outer` to ensure proper garbage collection tracking and package persistence.
- **Editor Context**: Niagara graph recompilation and system modification should be wrapped in `#if WITH_EDITOR` to ensure clean runtime vs editor compilation.

---

## 4. Conclusion

The C++ requirements and API usage for implementing `set_niagara_parameter` (Spec 6) are fully mapped and ready for implementation.

### Implementation Action Plan for Implementer:
1. **Headers**: Add `#include "NiagaraUserRedirectionParameterStore.h"`, `#include "NiagaraTypes.h"`, `#include "Curves/CurveFloat.h"`, and `#include "Curves/CurveLinearColor.h"` to `AgentFrameworkNiagaraActions.cpp`.
2. **Schema**: Update `AgentFramework/Resources/ToolSchemas/niagara_tools.json` to include `set_niagara_parameter` schema definition.
3. **Class Header**: Add `FAgentFrameworkActionResult ExecuteSetNiagaraParameter(...)` declaration to `AgentFrameworkNiagaraActions.h` and update `GetSupportedToolNames()` to list `TEXT("set_niagara_parameter")`.
4. **Class Implementation**:
   - Add parameter validation in `ValidateParams`.
   - Dispatch `ToolName == TEXT("set_niagara_parameter")` in `ExecuteAction`.
   - Implement `ExecuteSetNiagaraParameter` logic according to the audited API usage (primitive setting + `UCurveFloat`/`UCurveLinearColor` keyframe population).

---

## 5. Verification Method

To verify the audit findings and subsequent implementation:

1. **Build Script Execution**:
   Run plugin build script from repository root:
   ```powershell
   $env:uebp_UATMutexNoWait = '1'
   powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip
   ```
   Verify that `AgentFrameworkActions` compiles without header or symbol errors.

2. **Tool Execution Verification**:
   Invoke `set_niagara_parameter` against a test system (e.g. `/Game/Effects/NS_TestSystem`):
   - Set Float parameter: `ParameterName: "SpawnRate", DataType: "Float", Value: 100.0`
   - Set CurveFloat parameter: `ParameterName: "ScaleOverTime", DataType: "CurveFloat", CurveKeys: [{ "time": 0.0, "value": 1.0 }, { "time": 1.0, "value": 5.0 }]`
