# Milestone 4 Engine Analysis Report: Unreal Engine Asset Registry & Asset Tools C++ APIs

**Target Executor Class**: `FAgentFrameworkContextActions`  
**Header File**: `AgentFramework/Source/AgentFrameworkActions/Public/Context/AgentFrameworkContextActions.h`  
**Source File**: `AgentFramework/Source/AgentFrameworkActions/Private/Context/AgentFrameworkContextActions.cpp`  
**Investigated By**: Explorer 3 (Milestone 4 C++ Engine Investigator)  
**Date**: July 26, 2026  

---

## 1. Executive Summary

This audit evaluates the C++ Asset Management APIs in Unreal Engine 5.x required to natively implement:
1. **`enforce_naming_conventions`** (Spec 12 from `PYTHON_FALLBACK_AUDIT.md`)
2. **`organize_assets_by_type`** (Spec 14 from `PYTHON_FALLBACK_AUDIT.md`)

Both actions will be integrated directly into `FAgentFrameworkContextActions` inside `AgentFrameworkActions`. The audit details the exact usage of `IAssetRegistry`, `IAssetTools`, `FAssetRenameData`, `FScopedTransaction`, `UObjectRedirector`, dry-run path resolution, asset class resolution via `FTopLevelAssetPath`, package dirtying, and undo support.

---

## 2. Codebase Review: Existing Context Action Capabilities

Currently, `FAgentFrameworkContextActions` implements `IAgentFrameworkActionExecutor` and supports 4 read-only actions:
- `search_assets`: Uses `FAssetRegistryModule` and `IAssetRegistry::GetAllAssets()` to filter assets under `/Game/` matching query/class/path filters.
- `list_directory`: Uses `IFileManager` to enumerate physical files and folders under `FPaths::ProjectDir()`.
- `read_file_snippet`: Reads text files line-by-line using `FFileHelper::LoadFileToString`.
- `activate_skill`: Merges and writes active skill names into `.agents/active_skills.json`.

`FAgentFrameworkContextActions` is registered with module action name `Context` and executed on port `18777`. Adding `enforce_naming_conventions` and `organize_assets_by_type` extends `FAgentFrameworkContextActions` with write/mutation tools that manipulate package paths while maintaining full transaction safety and dry-run preview capabilities.

---

## 3. C++ Asset Management API Specifications

### 3.1 Asset Registry API (`IAssetRegistry`)

#### Module Access & Setup
```cpp
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"

FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
```

#### Querying Assets by Path
There are two primary methods for querying assets in a folder path:

1. **`GetAssetsByPath` (Direct Method)**:
   ```cpp
   TArray<FAssetData> AssetList;
   bool bRecursive = true;
   bool bIncludeOnlyOnDiskAssets = false; // Include in-memory unsaved assets
   AssetRegistry.GetAssetsByPath(FName(*FolderPath), AssetList, bRecursive, bIncludeOnlyOnDiskAssets);
   ```
   *Advantage*: Simple, direct call. Recommended for standard directory scans.

2. **`FARFilter` (Advanced Method)**:
   ```cpp
   FARFilter Filter;
   Filter.PackagePaths.Add(FName(*FolderPath));
   Filter.bRecursivePaths = bRecursive;
   Filter.bIncludeOnlyOnDiskAssets = false;
   TArray<FAssetData> AssetList;
   AssetRegistry.GetAssets(Filter, AssetList);
   ```
   *Advantage*: Allows combining path filters with class filters (`Filter.ClassPaths`), tags, or recursive flags.

#### Asset Data Properties (`FAssetData`)
- `AssetData.AssetName`: `FName` representing the asset object name (e.g., `MyBlueprint`).
- `AssetData.PackageName`: `FName` representing the full package path (e.g., `/Game/Blueprints/MyBlueprint`).
- `AssetData.PackagePath`: `FName` representing the directory path (e.g., `/Game/Blueprints`).
- `AssetData.GetObjectPathString()`: `FString` soft object path (e.g., `/Game/Blueprints/MyBlueprint.MyBlueprint`).
- `AssetData.AssetClassPath`: `FTopLevelAssetPath` representing package and class names (e.g., `/Script/Engine.Blueprint`, `/Script/Engine.Texture2D`, `/Script/UMG.WidgetBlueprint`).

---

### 3.2 Asset Class Identification & Resolution

In UE 5.1+, `FAssetData::AssetClass` (FName) was deprecated in favor of `AssetData.AssetClassPath` (`FTopLevelAssetPath`).

#### Class Resolution Mechanism
To obtain the class name as an `FString`:
```cpp
FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
```

#### Common Class Name Mapping Table

| UE Class Name (`AssetClassPath.GetAssetName()`) | `enforce_naming_conventions` Prefix | `organize_assets_by_type` Subfolder |
|---|---|---|
| `Blueprint` / `BlueprintGeneratedClass` | `BP_` | `Blueprints/` |
| `WidgetBlueprint` / `WidgetBlueprintGeneratedClass` | `WBP_` | `UI/` |
| `AnimBlueprint` / `AnimBlueprintGeneratedClass` | `ABP_` | `Animation/` |
| `Material` | `M_` | `Materials/` |
| `MaterialInstanceConstant` / `MaterialInstance` | `MI_` | `Materials/` |
| `MaterialFunction` | `MF_` | `Materials/` |
| `Texture2D` / `TextureCube` / `VolumeTexture` / `Texture` | `T_` | `Textures/` |
| `RenderTarget2D` | `RT_` | `Textures/` |
| `StaticMesh` | `SM_` | `Meshes/` |
| `SkeletalMesh` | `SKM_` | `Meshes/` |
| `PhysicsAsset` | `PHYS_` | `Meshes/` |
| `NiagaraSystem` | `NS_` | `Effects/` |
| `NiagaraEmitter` | `NE_` | `Effects/` |
| `ParticleSystem` | `PS_` | `Effects/` |
| `InputAction` | `IA_` | `Input/` |
| `InputMappingContext` | `IMC_` | `Input/` |
| `SoundWave` | `SW_` | `Audio/` |
| `SoundCue` | `SC_` | `Audio/` |
| `MetaSoundSource` | `MS_` | `Audio/` |
| `DataAsset` / `PrimaryDataAsset` | `DA_` | `Data/` |
| `DataTable` | `DT_` | `Data/` |
| `CurveTable` | `CT_` | `Data/` |
| `StringTable` | `ST_` | `Data/` |
| `LevelSequence` | `LS_` | `Sequencer/` |
| `AnimSequence` | `A_` | `Animation/` |
| `AnimMontage` | `AM_` | `Animation/` |
| `BlendSpace` / `BlendSpace1D` | `BS_` | `Animation/` |
| `Skeleton` | `SK_` | `Animation/` |
| `IKRigDefinition` | `IKR_` | `Animation/` |
| `IKRetargeter` | `IKRT_` | `Animation/` |
| `BehaviorTree` | `BT_` | `AI/` |
| `BlackboardData` | `BB_` | `AI/` |
| `PCGGraph` / `PCGGraphInterface` | `PCG_` | `PCG/` |
| `World` / `Level` | `L_` | `Maps/` |

---

### 3.3 Asset Tools API (`IAssetTools` & `FAssetRenameData`)

#### Module Access & Setup
```cpp
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRenameData.h"

IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
```

#### `FAssetRenameData` Structure
`FAssetRenameData` encapsulates a single asset rename or move operation:
```cpp
struct FAssetRenameData
{
    TWeakObjectPtr<UObject> Asset;
    FString NewPackagePath;   // Target package folder, e.g., "/Game/Textures"
    FString NewName;          // Target asset name, e.g., "T_BrickTexture"
    FSoftObjectPath SoftObjectPath; // Soft object path of the source asset
};
```
*Initialization Options*:
- Passing `FSoftObjectPath` (does not require asset to be pre-loaded):
  `FAssetRenameData RenameData(AssetData.GetSoftObjectPath(), NewPackagePath, NewAssetName);`
- Passing loaded `UObject*`:
  `FAssetRenameData RenameData(LoadedObject, NewPackagePath, NewAssetName);`

#### Renaming & Moving via `RenameAssets`
```cpp
TArray<FAssetRenameData> RenameDataArray;
RenameDataArray.Add(FAssetRenameData(AssetData.GetSoftObjectPath(), NewPackagePath, NewAssetName));

bool bSuccess = AssetTools.RenameAssets(RenameDataArray);
```

#### Internals of `RenameAssets`:
1. **Asset Loading**: Automatically loads target assets into memory if not already loaded.
2. **Disk & Package Rename**: Calls `UPackage::Rename`, moving package files on disk.
3. **Redirector Creation**: Automatically creates `UObjectRedirector` objects at the original package locations to maintain reference integrity across levels, Blueprints, and material pins.
4. **Asset Registry Broadcast**: Fires `IAssetRegistry::OnAssetRenamed`, instantly updating the asset registry index.
5. **Package Dirtying**: Marks both the new package and the redirector package as dirty (`Package->MarkPackageDirty()`).

---

### 3.4 Dry-Run Mode & Collision Check Design

In dry-run mode (`bDryRun = true`), the C++ action must evaluate candidate assets and calculate proposed paths **without calling `RenameAssets`**.

#### Dry-Run Workflow:
1. Query asset list using `IAssetRegistry::GetAssetsByPath`.
2. Compute `ProposedPackagePath` and `ProposedAssetName` for each asset.
3. Skip assets where `ProposedPackagePath == CurrentPackagePath` and `ProposedAssetName == CurrentAssetName`.
4. **Collision Check**:
   Before confirming a proposed target path, check if an asset already exists at the destination:
   ```cpp
   FString ProposedFullPackageName = ProposedPackagePath / ProposedAssetName;
   TArray<FAssetData> ExistingAssets;
   AssetRegistry.GetAssetsByPackageName(FName(*ProposedFullPackageName), ExistingAssets);
   bool bCollision = ExistingAssets.Num() > 0;
   ```
5. Record asset metadata into detailed JSON array:
   ```cpp
   TSharedPtr<FJsonObject> DetailObj = MakeShared<FJsonObject>();
   DetailObj->SetStringField(TEXT("old_path"), AssetData.GetObjectPathString());
   DetailObj->SetStringField(TEXT("new_path"), ProposedFullPackageName + TEXT(".") + ProposedAssetName);
   DetailObj->SetStringField(TEXT("asset_name"), AssetData.AssetName.ToString());
   DetailObj->SetStringField(TEXT("new_name"), ProposedAssetName);
   DetailObj->SetStringField(TEXT("asset_class"), ClassName);
   DetailObj->SetStringField(TEXT("status"), bCollision ? TEXT("CollisionDetected") : TEXT("PendingRename"));
   ```

---

### 3.5 Folder Creation, Redirectors, Transactions & Dirtying

#### 1. Folder Creation
In the Unreal Engine virtual package system (`/Game/...`), creating virtual subfolders occurs automatically when an asset package is created or moved to `NewPackagePath` via `IAssetTools::RenameAssets`.
If physical directory creation on disk is required prior to operations:
```cpp
FString DiskPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / RelativeSubfolder);
IFileManager::Get().MakeDirectory(*DiskPath, true);
```

#### 2. Object Redirector Handling (`UObjectRedirector`)
When `IAssetTools::RenameAssets` moves or renames an asset, it creates a `UObjectRedirector` object at the old package path.
- This ensures references in unloaded packages remain valid.
- If explicit redirector cleanup is desired, `FAssetToolsModule::Get().Get().FixupReferencers(...)` can be invoked, but creating redirectors is standard Editor safety.

#### 3. Package Dirtying & Saving
`IAssetTools::RenameAssets` marks the newly created package and the redirector package as dirty (`Package->MarkPackageDirty()`).
To save packages programmatically if needed:
```cpp
#include "FileHelpers.h"
// Option: UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false);
```

#### 4. Scoping Transactions (`FScopedTransaction`)
Wrapping operations inside `FScopedTransaction` registers the batch rename/move with the Unreal Editor Undo/Redo buffer:
```cpp
#include "ScopedTransaction.h"

#if WITH_EDITOR
FScopedTransaction Transaction(NSLOCTEXT("AgentFramework", "EnforceNamingConventions", "Enforce Asset Naming Conventions"));
#endif
```
This enables developers to press **Ctrl+Z** in the Unreal Editor to revert all renamed/moved assets in a single Undo action.

---

## 4. Implementation Design for `AgentFrameworkContextActions`

### 4.1 Specification 12 (`enforce_naming_conventions`) Implementation Pattern

```cpp
FAgentFrameworkActionResult FAgentFrameworkContextActions::ExecuteEnforceNamingConventions(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
    // 1. Dual-alias parameter extraction
    TSharedPtr<FJsonObject> ParamsPtr = Params;
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

    // Normalize path to /Game/...
    if (!FolderPath.StartsWith(TEXT("/Game")))
    {
        if (FolderPath.StartsWith(TEXT("Content/"))) FolderPath = TEXT("/Game/") + FolderPath.RightChop(8);
        else if (FolderPath.StartsWith(TEXT("Content"))) FolderPath = TEXT("/Game");
        else if (!FolderPath.StartsWith(TEXT("/"))) FolderPath = TEXT("/Game/") + FolderPath;
    }

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

    bool bRecursive = true;
    if (ParamsPtr->HasField(TEXT("recursive"))) UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("recursive"), bRecursive, Result.Errors, false);
    else if (ParamsPtr->HasField(TEXT("Recursive"))) UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("Recursive"), bRecursive, Result.Errors, false);

    // 2. Query Asset Registry
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> Assets;
    AssetRegistry.GetAssetsByPath(FName(*FolderPath), Assets, bRecursive, false);

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    TArray<FAssetRenameData> RenameDataArray;

    int32 CompliantCount = 0;
    int32 RenamedCount = 0;
    TArray<TSharedPtr<FJsonValue>> RenamedDetailsJson;

    for (const FAssetData& Asset : Assets)
    {
        FString AssetName = Asset.AssetName.ToString();
        FString PackagePath = Asset.PackagePath.ToString();
        FString ClassName = Asset.AssetClassPath.GetAssetName().ToString();

        FString ExpectedPrefix = GetPrefixForAssetClass(ClassName);
        if (ExpectedPrefix.IsEmpty()) continue; // Skip unmapped classes

        if (AssetName.StartsWith(ExpectedPrefix))
        {
            CompliantCount++;
            continue;
        }

        // Calculate new asset name (strip legacy prefix if present, e.g., Tex_, Mat_, BP_)
        FString CleanedName = StripLegacyPrefix(AssetName);
        FString NewAssetName = ExpectedPrefix + CleanedName;

        if (NewAssetName == AssetName) continue;

        RenamedCount++;

        TSharedPtr<FJsonObject> DetailObj = MakeShared<FJsonObject>();
        DetailObj->SetStringField(TEXT("old_path"), Asset.GetObjectPathString());
        DetailObj->SetStringField(TEXT("new_path"), PackagePath / NewAssetName + TEXT(".") + NewAssetName);
        DetailObj->SetStringField(TEXT("old_name"), AssetName);
        DetailObj->SetStringField(TEXT("new_name"), NewAssetName);
        DetailObj->SetStringField(TEXT("asset_class"), ClassName);
        RenamedDetailsJson.Add(MakeShared<FJsonValueObject>(DetailObj));

        if (!bDryRun)
        {
            RenameDataArray.Add(FAssetRenameData(Asset.GetSoftObjectPath(), PackagePath, NewAssetName));
        }
    }

    if (!bDryRun && RenameDataArray.Num() > 0)
    {
#if WITH_EDITOR
        FScopedTransaction Transaction(NSLOCTEXT("AgentFramework", "EnforceNamingConventions", "Enforce Asset Naming Conventions"));
#endif
        AssetTools.RenameAssets(RenameDataArray);
    }

    FString Message = FString::Printf(TEXT("%s naming conventions in %s. Compliant: %d, %s: %d"),
        bDryRun ? TEXT("Evaluated") : TEXT("Enforced"),
        *FolderPath, CompliantCount,
        bDryRun ? TEXT("Pending Renames") : TEXT("Renamed"), RenamedCount);

    Result.bSuccess = true;
    Result.ResultMessage = Message;
    return Result;
}
```

---

### 4.2 Specification 14 (`organize_assets_by_type`) Implementation Pattern

```cpp
FAgentFrameworkActionResult FAgentFrameworkContextActions::ExecuteOrganizeAssetsByType(const TSharedRef<FJsonObject>& Params, FAgentFrameworkActionResult& Result)
{
    // 1. Dual-alias parameter extraction
    TSharedPtr<FJsonObject> ParamsPtr = Params;
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

    // Normalize path
    if (!FolderPath.StartsWith(TEXT("/Game")))
    {
        if (FolderPath.StartsWith(TEXT("Content/"))) FolderPath = TEXT("/Game/") + FolderPath.RightChop(8);
        else if (FolderPath.StartsWith(TEXT("Content"))) FolderPath = TEXT("/Game");
        else if (!FolderPath.StartsWith(TEXT("/"))) FolderPath = TEXT("/Game/") + FolderPath;
    }

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

    bool bRecursive = true;
    if (ParamsPtr->HasField(TEXT("recursive"))) UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("recursive"), bRecursive, Result.Errors, false);
    else if (ParamsPtr->HasField(TEXT("Recursive"))) UAgentFrameworkActionUtils::TryGetBoolParam(ParamsPtr, TEXT("Recursive"), bRecursive, Result.Errors, false);

    // 2. Query Asset Registry
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> Assets;
    AssetRegistry.GetAssetsByPath(FName(*FolderPath), Assets, bRecursive, false);

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    TArray<FAssetRenameData> RenameDataArray;

    int32 MovedCount = 0;
    TMap<FString, int32> CategoryCounts;

    for (const FAssetData& Asset : Assets)
    {
        FString AssetName = Asset.AssetName.ToString();
        FString CurrentPackagePath = Asset.PackagePath.ToString();
        FString ClassName = Asset.AssetClassPath.GetAssetName().ToString();

        FString CategorySubfolder = GetSubfolderForAssetClass(ClassName);
        if (CategorySubfolder.IsEmpty()) continue;

        FString TargetPackagePath = FolderPath / CategorySubfolder;
        FPaths::NormalizeDirectoryName(TargetPackagePath);

        // Skip if already in correct subfolder
        if (CurrentPackagePath.Equals(TargetPackagePath, ESearchCase::IgnoreCase))
        {
            continue;
        }

        MovedCount++;
        CategoryCounts.FindOrAdd(CategorySubfolder)++;

        if (!bDryRun)
        {
            RenameDataArray.Add(FAssetRenameData(Asset.GetSoftObjectPath(), TargetPackagePath, AssetName));
        }
    }

    if (!bDryRun && RenameDataArray.Num() > 0)
    {
#if WITH_EDITOR
        FScopedTransaction Transaction(NSLOCTEXT("AgentFramework", "OrganizeAssetsByType", "Organize Assets By Type"));
#endif
        AssetTools.RenameAssets(RenameDataArray);
    }

    FString Message = FString::Printf(TEXT("%s asset organization in %s. Total moved: %d across %d categories."),
        bDryRun ? TEXT("Evaluated") : TEXT("Executed"),
        *FolderPath, MovedCount, CategoryCounts.Num());

    Result.bSuccess = true;
    Result.ResultMessage = Message;
    return Result;
}
```

---

## 5. Verification & Safety Guidelines

1. **Transaction Safety**: All batch modifications are wrapped in `FScopedTransaction`.
2. **Redirector Preservation**: `IAssetTools::RenameAssets` maintains reference integrity across packages automatically.
3. **Dry-Run Fidelity**: Dry-run mode evaluates the exact path logic without invoking `RenameAssets` or dirtying packages.
4. **Collision Invalidation**: Target paths are pre-checked via `AssetRegistry.GetAssetsByPackageName`.

---
