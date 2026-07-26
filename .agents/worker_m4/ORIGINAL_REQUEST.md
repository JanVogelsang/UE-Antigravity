## 2026-07-26T16:15:52Z

<USER_REQUEST>
You are Worker for Milestone 4 (Context Actions: enforce_naming_conventions Spec 12 & organize_assets_by_type Spec 14).

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Your task is to implement the native C++ actions `enforce_naming_conventions` (Spec 12) and `organize_assets_by_type` (Spec 14) and update `context_tools.json`.

Files to modify:
1. `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\AgentFramework\Source\AgentFrameworkActions\Public\Context\AgentFrameworkContextActions.h`
2. `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\AgentFramework\Source\AgentFrameworkActions\Private\Context\AgentFrameworkContextActions.cpp`
3. `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\AgentFramework\Resources\ToolSchemas\context_tools.json`

Read the Explorer analysis reports:
- `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\explorer_m4_code\analysis.md`
- `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\explorer_m4_schema\analysis.md`
- `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\explorer_m4_engine\analysis.md`

Requirements:
1. Header (`AgentFrameworkContextActions.h`):
   - Declare static action methods `ExecuteEnforceNamingConventions` and `ExecuteOrganizeAssetsByType`.
2. Source (`AgentFrameworkContextActions.cpp`):
   - Add `"enforce_naming_conventions"` and `"organize_assets_by_type"` to `GetSupportedToolNames()`.
   - Dispatch both actions in `ExecuteAction()`.
   - Implement `enforce_naming_conventions`:
     * Dual alias parsing (`folder_path`/`directory_path`/`target_folder`/`FolderPath`, `dry_run`/`dry_run_mode`/`DryRun`, `custom_rules`/`CustomRules`, `recursive`/`Recursive`).
     * Standard asset prefix mapping (`Blueprint` -> `BP_`, `WidgetBlueprint` -> `WBP_`, `AnimBlueprint` -> `ABP_`, `Material` -> `M_`, `MaterialInstanceConstant`/`MaterialInstance` -> `MI_`, `Texture2D`/`Texture` -> `T_`, `StaticMesh` -> `SM_`, `SkeletalMesh` -> `SKM_`, `NiagaraSystem` -> `NS_`, `NiagaraEmitter` -> `NE_`, `InputAction` -> `IA_`, `InputMappingContext` -> `IMC_`, `SoundWave` -> `SW_`, `SoundCue` -> `SC_`, `DataAsset`/`PrimaryDataAsset` -> `DA_`, `DataTable` -> `DT_`, `LevelSequence` -> `LS_`, etc.).
     * Use `IAssetRegistry::GetAssetsByPath` and `IAssetTools::RenameAssets` with `FAssetRenameData` for actual renames (`bDryRun == false`).
     * Simulate renames and return structured list when `bDryRun == true`.
     * Wrap in `FScopedTransaction`.
   - Implement `organize_assets_by_type`:
     * Dual alias parsing (`folder_path`/`directory_path`/`source_path`/`FolderPath`, `dry_run`/`dry_run_mode`/`DryRun`, `create_subfolders`/`CreateSubfolders`, `recursive`/`Recursive`).
     * Target subfolder mapping (`Blueprints`, `Materials`, `Textures`, `UI`, `Effects`, `Input`, `Audio`, `Meshes`, `Animation`, `Data`, `Sequencer`, `PCG`).
     * Check if asset is already in correct subfolder before moving. Move assets via `IAssetTools::RenameAssets` (`bDryRun == false`).
     * Simulate moves when `bDryRun == true`.
     * Wrap in `FScopedTransaction`.
3. JSON Schema (`context_tools.json`):
   - Add complete schema entries for `enforce_naming_conventions` and `organize_assets_by_type` supporting both snake_case and PascalCase parameter keys.
4. Report changes in your handoff report at `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\worker_m4\handoff.md` and send a message back.
</USER_REQUEST>
