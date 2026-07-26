# Milestone 4 Investigation Report: Context Actions (Specs 12 & 14)

**Target Executor Class**: `FAgentFrameworkContextActions`  
**Header File**: `AgentFramework/Source/AgentFrameworkActions/Public/Context/AgentFrameworkContextActions.h`  
**Source File**: `AgentFramework/Source/AgentFrameworkActions/Private/Context/AgentFrameworkContextActions.cpp`  
**Audit Document**: `Documentation/PYTHON_FALLBACK_AUDIT.md` (Specs 12 & 14)  
**Investigated By**: Explorer 1  
**Date**: July 26, 2026  

---

## 1. Executive Summary & Architectural Overview

The `FAgentFrameworkContextActions` class serves as the core C++ action executor for project context, discovery, directory inspection, and asset registry actions within the in-editor `AgentFrameworkActions` plugin on port `18777`. Currently, it supports 4 native tools:
1. `search_assets`
2. `list_directory`
3. `read_file_snippet`
4. `activate_skill`

Milestone 4 requires extending `FAgentFrameworkContextActions` with two new native asset management and hygiene tools to eliminate Python fallbacks:
- **Spec 12 (`enforce_naming_conventions`)**: Scans asset registry paths, checks class-to-prefix standards (`BP_`, `M_`, `MI_`, `T_`, `NS_`, `NE_`, `WBP_`, `IA_`, `IMC_`, `SW_`, `SM_`, `SKM_`, `DA_`, `DT_`, `LS_`, etc.), renames non-compliant assets via C++ `AssetTools`, and reports compliance metrics.
- **Spec 14 (`organize_assets_by_type`)**: Automatically categorizes and moves mixed assets in content directories into standardized type-specific subfolders (`Blueprints/`, `Materials/`, `Textures/`, `UI/`, `Effects/`, `Input/`, `Audio/`, `Meshes/`, `Animation/`, `Data/`, `Sequencer/`, `PCG/`) using Unreal C++ `AssetTools`.

---

## 2. Header and Implementation Declarations required

### 2.1 Header Updates (`AgentFrameworkContextActions.h`)
In `AgentFrameworkContextActions.h`:
- Add private member function declarations:
```cpp
FAgentFrameworkActionResult ExecuteEnforceNamingConventions(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
FAgentFrameworkActionResult ExecuteOrganizeAssetsByType(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result);
```

### 2.2 Tool Registration & Dispatch Updates (`AgentFrameworkContextActions.cpp`)
1. In `GetSupportedToolNames()`: Add `TEXT("enforce_naming_conventions")` and `TEXT("organize_assets_by_type")`.
2. In `ExecuteAction()`: Add routing branches for `Action == TEXT("enforce_naming_conventions")` and `Action == TEXT("organize_assets_by_type")`.

---

## 3. Specification 12: `enforce_naming_conventions` Analysis & Design

### 3.1 Parameter Names & Dual-Alias Strategy
`enforce_naming_conventions` must support snake_case and PascalCase parameter aliases:

| Parameter Purpose | Primary Parameter | Aliases / Alternatives | Type | Default |
|---|---|---|---|---|
| Target Folder Path | `folder_path` | `directory_path`, `target_folder`, `FolderPath` | FString | Required |
| Dry Run Mode | `dry_run` | `dry_run_mode`, `DryRun` | bool | `false` |
| Recursive Scan | `recursive` | `Recursive` | bool | `true` |
| Custom Rules | `custom_rules` | `CustomRules` | Object / Map | Empty |

**Extraction Pattern**:
```cpp
// 1. Folder Path Extraction
FString FolderPath;
UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("folder_path"), FolderPath, Result.Errors, false);
if (FolderPath.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("directory_path"), FolderPath, Result.Errors, false);
if (FolderPath.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("target_folder"), FolderPath, Result.Errors, false);
if (FolderPath.IsEmpty()) UAgentFrameworkActionUtils::TryGetStringParam(ParamsPtr, TEXT("FolderPath"), FolderPath, Result.Errors, false);

if (FolderPath.IsEmpty())
{
    Result.Errors.Add(TEXT("Parameter 'folder_path' (or 'directory_path' / 'target_folder' / 'FolderPath') is required."));
    return Result;
}

// Normalize folder path to starting with /Game/
if (!FolderPath.StartsWith(TEXT("/Game")))
{
    if (FolderPath.StartsWith(TEXT("Content/"))) FolderPath = TEXT("/Game/") + FolderPath.RightChop(8);
    else if (FolderPath.StartsWith(TEXT("Content"))) FolderPath = TEXT("/Game");
    else if (!FolderPath.StartsWith(TEXT("/"))) FolderPath = TEXT("/Game/") + FolderPath;
}

// 2. Dry Run Flag Extraction
bool bDryRun = false;
UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("dry_run"), bDryRun, Result.Errors, false);
if (!ParamsPtr->HasField(TEXT("dry_run")))
{
    UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("dry_run_mode"), bDryRun, Result.Errors, false);
    if (!ParamsPtr->HasField(TEXT("dry_run_mode")))
    {
        UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("DryRun"), bDryRun, Result.Errors, false);
    }
}

// 3. Recursive Flag Extraction
bool bRecursive = true;
if (ParamsPtr->HasField(TEXT("recursive"))) UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("recursive"), bRecursive, Result.Errors, false);
else if (ParamsPtr->HasField(TEXT("Recursive"))) UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("Recursive"), bRecursive, Result.Errors, false);
```

### 3.2 Asset Class to Prefix Standard Mapping Table

A C++ `TMap<FString, FString>` or static resolver maps UE asset class names to standard prefixes:

| Asset Class / Category | Standard Prefix | Notes |
|---|---|---|
| `Blueprint`, `BlueprintGeneratedClass` | `BP_` | Blueprint Actors & Objects |
| `WidgetBlueprint`, `WidgetBlueprintGeneratedClass` | `WBP_` | UMG User Widgets |
| `AnimBlueprint`, `AnimBlueprintGeneratedClass` | `ABP_` | Animation Blueprints |
| `Material` | `M_` | Base Materials |
| `MaterialInstanceConstant`, `MaterialInstance` | `MI_` | Material Instances |
| `MaterialFunction` | `MF_` | Material Functions |
| `MaterialParameterCollection` | `MPC_` | Material Parameter Collections |
| `Texture2D`, `TextureCube`, `VolumeTexture`, `Texture` | `T_` | Textures |
| `RenderTarget2D` | `RT_` | Render Targets |
| `StaticMesh` | `SM_` | Static Meshes |
| `SkeletalMesh` | `SKM_` | Skeletal Meshes |
| `PhysicsAsset` | `PHYS_` | Physics Assets |
| `NiagaraSystem` | `NS_` | Niagara Systems |
| `NiagaraEmitter` | `NE_` | Niagara Emitters |
| `ParticleSystem` | `PS_` | Cascade Particle Systems |
| `InputAction` | `IA_` | Enhanced Input Actions |
| `InputMappingContext` | `IMC_` | Enhanced Input Mapping Contexts |
| `SoundWave` | `SW_` | Sound Waves |
| `SoundCue` | `SC_` | Sound Cues |
| `SoundAttenuation` | `SA_` | Sound Attenuation |
| `MetaSoundSource` | `MS_` | MetaSounds |
| `DataAsset`, `PrimaryDataAsset` | `DA_` | Data Assets |
| `DataTable` | `DT_` | Data Tables |
| `CurveTable` | `CT_` | Curve Tables |
| `StringTable` | `ST_` | String Tables |
| `LevelSequence` | `LS_` | Level Sequences |
| `AnimSequence` | `A_` | Animation Sequences |
| `AnimMontage` | `AM_` | Animation Montages |
| `BlendSpace`, `BlendSpace1D` | `BS_` | Blend Spaces |
| `Skeleton` | `SK_` | Skeletons |
| `IKRigDefinition` | `IKR_` | IK Rigs |
| `IKRetargeter` | `IKRT_` | IK Retargeters |
| `BehaviorTree` | `BT_` | Behavior Trees |
| `BlackboardData` | `BB_` | Blackboard Data |
| `PCGGraph`, `PCGGraphInterface` | `PCG_` | PCG Graphs |

### 3.3 Renaming & Compliance Execution Logic
1. Load Asset Registry: `FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");`
2. Perform search under `FolderPath` using `AssetRegistry.GetAssetsByPath(*FolderPath, Assets, bRecursive);`.
3. For each `FAssetData`:
   - Obtain `AssetName` = `Asset.AssetName.ToString()`, `AssetClass` = `Asset.AssetClassPath.GetAssetName().ToString()`, `PackagePath` = `Asset.PackagePath.ToString()`.
   - Determine `ExpectedPrefix` from map (or `custom_rules`). Skip if class is unmapped or unknown.
   - Check if `AssetName.StartsWith(ExpectedPrefix)`. If true -> increment `CompliantCount`.
   - If false:
     - Compute new asset name:
       - Check if `AssetName` starts with a common misspelled/legacy prefix (e.g. `bp_`, `Tex_`, `Mat_`, `Mesh_`, `SMesh_`). If so, strip the legacy prefix before prepending `ExpectedPrefix`.
       - Otherwise, `NewAssetName = ExpectedPrefix + AssetName`.
     - New Package Path = `PackagePath / NewAssetName`.
     - In `bDryRun` mode:
       - Record rename pair (`old_path`, `new_path`, `old_name`, `new_name`, `asset_class`) in `renamed_details` array.
       - Increment `RenamedCount`.
     - In execution mode (`!bDryRun`):
       - Load object: `UObject* AssetObj = Asset.GetAsset();`
       - Create `FAssetRenameData RenameData(AssetObj, PackagePath, NewAssetName);`
       - Execute rename using `FAssetToolsModule::Get().Get().RenameAssets({ RenameData });`
       - Record in `renamed_details` array and increment `RenamedCount`.

---

## 4. Specification 14: `organize_assets_by_type` Analysis & Design

### 4.1 Parameter Names & Dual-Alias Strategy
`organize_assets_by_type` must support snake_case and PascalCase parameter aliases:

| Parameter Purpose | Primary Parameter | Aliases / Alternatives | Type | Default |
|---|---|---|---|---|
| Target Folder Path | `folder_path` | `directory_path`, `source_path`, `FolderPath` | FString | Required |
| Dry Run Mode | `dry_run` | `dry_run_mode`, `DryRun` | bool | `false` |
| Create Subfolders | `create_subfolders` | `CreateSubfolders` | bool | `true` |
| Recursive Scan | `recursive` | `Recursive` | bool | `true` |

### 4.2 Category & Target Subfolder Mapping Table

| Category / Target Subfolder | Associated Asset Classes |
|---|---|
| `Blueprints` | `Blueprint`, `BlueprintGeneratedClass` |
| `Materials` | `Material`, `MaterialInstanceConstant`, `MaterialInstanceDynamic`, `MaterialInstance`, `MaterialFunction`, `MaterialParameterCollection` |
| `Textures` | `Texture2D`, `TextureCube`, `VolumeTexture`, `RenderTarget2D`, `Texture` |
| `UI` | `WidgetBlueprint`, `WidgetBlueprintGeneratedClass`, `SlateWidgetStyleAsset`, `Font`, `FontFace` |
| `Effects` | `NiagaraSystem`, `NiagaraEmitter`, `ParticleSystem` |
| `Input` | `InputAction`, `InputMappingContext` |
| `Audio` | `SoundWave`, `SoundCue`, `SoundAttenuation`, `SoundConcurrency`, `MetaSoundSource` |
| `Meshes` | `StaticMesh`, `SkeletalMesh`, `PhysicsAsset` |
| `Animation` | `AnimSequence`, `AnimMontage`, `AnimBlueprint`, `BlendSpace`, `BlendSpace1D`, `Skeleton`, `IKRigDefinition`, `IKRetargeter` |
| `Data` | `DataAsset`, `PrimaryDataAsset`, `DataTable`, `CurveTable`, `StringTable` |
| `Sequencer` | `LevelSequence` |
| `PCG` | `PCGGraph`, `PCGGraphInterface` |

### 4.3 Asset Relocation & Category Organization Logic
1. Load Asset Registry under `FolderPath`.
2. For each `FAssetData`:
   - Determine `SubfolderName` from Category Mapping Table (e.g. `"Blueprints"`).
   - Target Package Directory = `FolderPath / SubfolderName`.
   - Current Package Path = `Asset.PackagePath.ToString()`.
   - **Guard**: If `CurrentPackagePath == TargetPackageDirectory` or `CurrentPackagePath.StartsWith(TargetPackageDirectory + TEXT("/"))`, the asset is ALREADY inside the correct subfolder! Skip moving.
   - If not in subfolder:
     - New Package Path = `FolderPath / SubfolderName`.
     - In `bDryRun` mode:
       - Update category counter in `category_counts` JSON object (e.g. `Blueprints: +1`).
       - Record relocation details in `moved_details` array.
       - Increment `MovedCount`.
     - In execution mode (`!bDryRun`):
       - Prepare `FAssetRenameData RenameData(Asset.GetAsset(), NewPackagePath, Asset.AssetName.ToString());`.
       - Execute batch relocation using `FAssetToolsModule::Get().Get().RenameAssets(BatchRenameData);`.
       - Update category counter and details array.

---

## 5. JSON Response Structures & Error Handling

### 5.1 `enforce_naming_conventions` JSON Result Format
```json
{
  "bSuccess": true,
  "folder_path": "/Game/Art/Environment",
  "total_scanned_count": 45,
  "compliant_count": 30,
  "renamed_assets_count": 15,
  "dry_run": false,
  "renamed_details": [
    {
      "asset_class": "Blueprint",
      "old_name": "PlayerPawn",
      "new_name": "BP_PlayerPawn",
      "old_path": "/Game/Art/Environment/PlayerPawn",
      "new_path": "/Game/Art/Environment/BP_PlayerPawn"
    }
  ],
  "ResultMessage": "Enforced naming conventions in /Game/Art/Environment. Scanned: 45, Compliant: 30, Renamed: 15 (DryRun: false)."
}
```

### 5.2 `organize_assets_by_type` JSON Result Format
```json
{
  "bSuccess": true,
  "folder_path": "/Game/Imports",
  "total_scanned_count": 50,
  "moved_assets_count": 35,
  "dry_run": false,
  "category_counts": {
    "Blueprints": 5,
    "Materials": 10,
    "Textures": 15,
    "Audio": 5
  },
  "moved_details": [
    {
      "asset_name": "Chair_StaticMesh",
      "asset_class": "StaticMesh",
      "category": "Meshes",
      "old_path": "/Game/Imports/Chair_StaticMesh",
      "new_path": "/Game/Imports/Meshes/Chair_StaticMesh"
    }
  ],
  "ResultMessage": "Organized 35 assets into subfolders under /Game/Imports (DryRun: false)."
}
```

---

## 6. Implementation Verification Method & Plan

1. **Compilation**:
   Run `build_plugin.ps1` from root repository to build `AgentFrameworkActions` C++ plugin.
2. **Automated E2E Tests**:
   Run `powershell -File .\Tests\run_tests.ps1` to execute integration tests against port 18777.
3. **Manual Verification Prompt**:
   - Call `enforce_naming_conventions` on a test folder in `AgentFrameworkTest`.
   - Call `organize_assets_by_type` on a test folder in `AgentFrameworkTest`.
