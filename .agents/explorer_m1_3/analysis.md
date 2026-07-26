# Analysis & Migration Plan: setup-replication & niagara-authoring Skills

## 1. Context & Objectives
- **Milestone**: Milestone 1 — Skill Documentation Migration
- **Target Skills**:
  1. `UnrealEngine/skills/setup-replication/SKILL.md`
  2. `UnrealEngine/skills/niagara-authoring/SKILL.md`
- **Reference**: `Documentation/PYTHON_FALLBACK_AUDIT.md` (Specifications 3, 4, & 6)
- **Goal**: Analyze the exact documentation changes required to incorporate native C++ MCP tool routes (`configure_actor_replication`, `set_variable_replication`, and `set_niagara_parameter`) into the target skill files, eliminating Python fallbacks and manual workarounds.

---

## 2. Detailed Investigation & Findings

### 2.1 Skill 1: `setup-replication/SKILL.md`

#### Current State Analysis
- **File Path**: `UnrealEngine/skills/setup-replication/SKILL.md` (51 lines)
- **Current Coverage**: Focuses entirely on C++ code snippets (`bReplicates = true;`, `GetLifetimeReplicatedProps`, `UPROPERTY(Replicated)`, `UPROPERTY(ReplicatedUsing=OnRep_Shield)`, Server RPCs).
- **Documentation Gap**: Does not include native C++ Editor MCP tool routes for configuring network replication directly on Blueprint actor assets (`.uasset`) or Blueprint member variables in editor without manually writing C++ base code.

#### Identified Tool Replacements (from `PYTHON_FALLBACK_AUDIT.md`)
1. **`configure_actor_replication`** (Audit Specification 3):
   - **Module**: `AgentFrameworkBlueprintActions`
   - **Capability**: Sets CDO network replication default properties on Blueprint actor assets.
   - **Key Parameters**:
     - `TargetAsset` (string, required): Long package path of Blueprint actor (e.g. `'/Game/Blueprints/BP_NetworkPawn'`)
     - `bReplicates` (boolean, default: `true`): Enable network replication for actor
     - `bReplicateMovement` (boolean, default: `true`): Enable movement replication
     - `NetDormancy` (enum string, default: `'DORM_Never'`): `'DORM_Never'`, `'DORM_Awake'`, `'DORM_DormantAll'`, `'DORM_DormantPartial'`, `'DORM_Initial'`
     - `NetUpdateFrequency` (number, default: `100.0`): Updates per second (Hz)
     - `NetPriority` (number, default: `1.0`): Network replication priority weight
   
2. **`set_variable_replication`** (Audit Specification 4):
   - **Module**: `AgentFrameworkBlueprintActions`
   - **Capability**: Configures variable replication mode, RepNotify callback generation, and lifetime replication conditions on Blueprint member variables.
   - **Key Parameters**:
     - `TargetAsset` (string, required): Long package path of Blueprint asset
     - `VariableName` (string, required): Member variable name (e.g. `'Health'`, `'Shield'`)
     - `ReplicationType` (enum string, required): `'None'`, `'Replicated'`, `'RepNotify'`
     - `RepNotifyFunc` (string, optional): Custom RepNotify callback function name (e.g. `'OnRep_Shield'`); automatically generates callback graph if specified
     - `ReplicationCondition` (enum string, default: `'COND_None'`): Lifetime condition (`'COND_None'`, `'COND_InitialOnly'`, `'COND_OwnerOnly'`, `'COND_SkipOwner'`, `'COND_SimulatedOnly'`, `'COND_AutonomousOnly'`, `'COND_Custom'`)

#### Recommended Documentation Enhancements for `setup-replication/SKILL.md`
- Add a new dedicated section for **Blueprint Editor Native Tool Routes** alongside the existing C++ class patterns.
- Section 1 (Actor Level Replication): Provide both C++ constructor code and `configure_actor_replication` tool call JSON example.
- Section 3 (Variable Level Replication): Provide both C++ `UPROPERTY` code and `set_variable_replication` tool call JSON examples for standard replication, RepNotify callbacks, and lifetime conditions (`COND_OwnerOnly`, `COND_SkipOwner`).

---

### 2.2 Skill 2: `niagara-authoring/SKILL.md`

#### Current State Analysis
- **File Path**: `UnrealEngine/skills/niagara-authoring/SKILL.md` (41 lines)
- **Current Coverage**: Documents 6 native Niagara C++ tools (`create_niagara_system`, `add_niagara_emitter`, `add_niagara_module`, `set_niagara_module_pin`, `compile_niagara_system`, `capture_niagara_system_isolated`).
- **Documentation Gap**: Step 4 ("Parameter and Curve Binding") and Step 6 ("Temporal Vision Capture loop") only mention `set_niagara_module_pin` (which mutates module-level input pins). They lack native tool documentation for `set_niagara_parameter`, which handles System/Emitter-level User parameter overrides (`User.SpawnRate`, `User.PrimaryColor`) and dynamic float/color curve data (`UCurveFloat`, `UCurveLinearColor`).

#### Identified Tool Replacements (from `PYTHON_FALLBACK_AUDIT.md`)
1. **`set_niagara_parameter`** (Audit Specification 6):
   - **Module**: `AgentFrameworkNiagaraActions`
   - **Capability**: Sets System, Emitter, or User level parameter overrides and keyframe curves via `UNiagaraUserRedirectionParameterStore`.
   - **Key Parameters**:
     - `SystemAsset` (string, required): Long package path of UNiagaraSystem asset (e.g. `'/Game/VFX/NS_Explosion'`)
     - `ParameterScope` (enum string, default: `'User'`): `'User'`, `'System'`, `'Emitter'`
     - `ParameterName` (string, required): Parameter name (e.g. `'SpawnRate'`, `'PrimaryColor'`)
     - `DataType` (enum string, default: `'Float'`): `'Float'`, `'Vector2'`, `'Vector3'`, `'LinearColor'`, `'Bool'`, `'Int32'`, `'CurveFloat'`, `'CurveLinearColor'`
     - `Value` (any): Constant value (scalar, array `[x,y,z]`, RGBA color object/array)
     - `CurveKeys` (array of objects): `[{ "Time": 0.0, "Value": 0.0 }, { "Time": 1.0, "Value": 100.0 }]` for float/color curves

#### Recommended Documentation Enhancements for `niagara-authoring/SKILL.md`
- Update **Step 4: Parameter and Curve Binding**: Expand to cover both module stack inputs (`set_niagara_module_pin`) and System/Emitter User parameters and curves (`set_niagara_parameter`).
- Provide concrete JSON payload examples for scalar User parameters (`User.SpawnRate`), color User parameters (`User.PrimaryColor`), and dynamic curve keys (`CurveFloat`).
- Update **Step 6: Temporal Vision Capture loop**: Clarify that both `set_niagara_module_pin` and `set_niagara_parameter` can be used to tweak parameters during visual evaluation.

---

## 3. Step-by-Step Editing Instructions for Implementer

### 3.1 Instructions for `setup-replication/SKILL.md`

Target File: `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/UnrealEngine/skills/setup-replication/SKILL.md`

#### Edit 1: Update Steps 1 and 3 to include Native MCP Tool Routes

Replace lines 10-43 of `UnrealEngine/skills/setup-replication/SKILL.md` with:

```markdown
## Steps

### Option A: Blueprint Native Tool Routes (In-Editor Asset Configuration)

1. **Configure Actor Network Replication (`configure_actor_replication`)**:
   Use `configure_actor_replication` to set network replication defaults on a Blueprint actor CDO:
   ```json
   {
     "TargetAsset": "/Game/Blueprints/BP_NetworkPawn",
     "bReplicates": true,
     "bReplicateMovement": true,
     "NetDormancy": "DORM_Never",
     "NetUpdateFrequency": 100.0,
     "NetPriority": 1.0
   }
   ```

2. **Configure Variable Replication & RepNotify (`set_variable_replication`)**:
   
   - **Standard Variable Replication**:
     ```json
     {
       "TargetAsset": "/Game/Blueprints/BP_NetworkPawn",
       "VariableName": "Health",
       "ReplicationType": "Replicated",
       "ReplicationCondition": "COND_None"
     }
     ```

   - **Replication with RepNotify Callback**:
     Setting `ReplicationType` to `"RepNotify"` and specifying `RepNotifyFunc` will automatically generate the `OnRep_Shield` callback graph:
     ```json
     {
       "TargetAsset": "/Game/Blueprints/BP_NetworkPawn",
       "VariableName": "Shield",
       "ReplicationType": "RepNotify",
       "RepNotifyFunc": "OnRep_Shield",
       "ReplicationCondition": "COND_None"
     }
     ```

   - **Replication with Lifetime Condition (Owner Only / Skip Owner)**:
     ```json
     {
       "TargetAsset": "/Game/Blueprints/BP_NetworkPawn",
       "VariableName": "SecretData",
       "ReplicationType": "Replicated",
       "ReplicationCondition": "COND_OwnerOnly"
     }
     ```

### Option B: C++ Code Patterns (Base Class Implementation)

1. **Constructor**: Enable replication
   ```cpp
   bReplicates = true;
   SetReplicateMovement(true);
   ```

2. **Add GetLifetimeReplicatedProps**:
   ```cpp
   void {{arg}}::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
   {
       Super::GetLifetimeReplicatedProps(OutLifetimeProps);
       DOREPLIFETIME({{arg}}, Health);
       DOREPLIFETIME_CONDITION({{arg}}, bIsVisible, COND_OwnerOnly);
   }
   ```

3. **Mark properties for replication**:
   
   **Standard Replication (No callback)**:
   ```cpp
   UPROPERTY(Replicated)
   float Health;
   ```
   
   **Replication with RepNotify Callback (Client side effect)**:
   ```cpp
   UPROPERTY(ReplicatedUsing=OnRep_Shield)
   float Shield;
   
   UFUNCTION()
   void OnRep_Shield(float OldShield);
   ```
```

---

### 3.2 Instructions for `niagara-authoring/SKILL.md`

Target File: `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/UnrealEngine/skills/niagara-authoring/SKILL.md`

#### Edit 1: Update Step 4 and Step 6 in `niagara-authoring/SKILL.md`

Replace lines 26-36 of `UnrealEngine/skills/niagara-authoring/SKILL.md` with:

```markdown
4. **Parameter and Curve Binding**:
   - **Module Stack Input Pins (`set_niagara_module_pin`)**:
     Configure constants or time-value curves for module inputs (e.g., setting size/color curves over particle life).
   
   - **System/Emitter Level User Parameters & Curves (`set_niagara_parameter`)**:
     Use `set_niagara_parameter` to set exposed User parameters or dynamic curve parameter overrides on the system store:
     
     *Scalar / Color User Parameter*:
     ```json
     {
       "SystemAsset": "/Game/VFX/NS_Explosion",
       "ParameterScope": "User",
       "ParameterName": "SpawnRate",
       "DataType": "Float",
       "Value": 500.0
     }
     ```

     *Float Curve Override*:
     ```json
     {
       "SystemAsset": "/Game/VFX/NS_Explosion",
       "ParameterScope": "User",
       "ParameterName": "SizeOverLife",
       "DataType": "CurveFloat",
       "CurveKeys": [
         { "Time": 0.0, "Value": 10.0 },
         { "Time": 0.5, "Value": 50.0 },
         { "Time": 1.0, "Value": 0.0 }
       ]
     }
     ```
    
5. **Compilation Verification**:
   Call `compile_niagara_system` to build the script bytecode. Analyze the returned compile warnings and error messages to verify structural correctness.

6. **Temporal Vision Capture loop**:
   Call `capture_niagara_system_isolated` to retrieve a stitched 2x2 grid image representing the start, middle, peak, and dissipation keyframes of the simulation.
   * Examine time labels (`t = 0.5s`) and the visual scale indicator bar (e.g. `1m` or `10cm`) to evaluate size and speed.
   * Make adjustments to parameters using `set_niagara_module_pin` or `set_niagara_parameter` and iterate until target look matches user prompts/reference images.
```

---

## 4. Verification & Alignment Check
- **`configure_actor_replication`**: Verified against Audit Spec 3. Covers `bReplicates`, `bReplicateMovement`, `NetDormancy`, `NetUpdateFrequency`, `NetPriority`.
- **`set_variable_replication`**: Verified against Audit Spec 4. Covers `Replicated` vs `RepNotify`, custom RepNotify callback auto-generation (`RepNotifyFunc`), lifetime conditions (`COND_OwnerOnly`, `COND_SkipOwner`, etc.).
- **`set_niagara_parameter`**: Verified against Audit Spec 6. Covers User/System/Emitter parameters, scalar/color values, and float/color curve keyframe arrays.
