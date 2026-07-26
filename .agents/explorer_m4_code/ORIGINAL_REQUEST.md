## 2026-07-26T16:14:19Z

You are Explorer 1 for Milestone 4 (Context Actions: enforce_naming_conventions Spec 12 & organize_assets_by_type Spec 14).
Your task is to investigate the C++ codebase for Context Actions and Specifications 12 and 14 in PYTHON_FALLBACK_AUDIT.md.

Read:
1. `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\AgentFramework\Source\AgentFrameworkActions\Public\Context\AgentFrameworkContextActions.h`
2. `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\AgentFramework\Source\AgentFrameworkActions\Private\Context\AgentFrameworkContextActions.cpp`
3. `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\Documentation\PYTHON_FALLBACK_AUDIT.md` (Specs 12 & 14)

Analyze:
- Spec 12 (`enforce_naming_conventions`): Parameter names (`folder_path`/`directory_path`/`target_folder`/`FolderPath`, `dry_run`/`dry_run_mode`/`DryRun`, `custom_rules`/`CustomRules`, `recursive`/`Recursive`). Naming conventions mapping asset types (Blueprint, Material, MaterialInstance, Texture, NiagaraSystem, NiagaraEmitter, WidgetBlueprint, InputAction, InputMappingContext, SoundWave, etc.) to standard prefixes (`BP_`, `M_`, `MI_`, `T_`, `NS_`, `NE_`, `WBP_`, `IA_`, `IMC_`, `SW_`/`S_`). Handling renaming logic and reporting renamed vs compliant assets.
- Spec 14 (`organize_assets_by_type`): Parameter names (`folder_path`/`directory_path`/`source_path`/`FolderPath`, `dry_run`/`dry_run_mode`/`DryRun`, `create_subfolders`/`CreateSubfolders`, `recursive`/`Recursive`). Organization mapping asset types to target subfolders (`Blueprints`, `Materials`, `Textures`, `UI`, `Effects`, `Input`, `Audio`, etc.) under the target directory. Moving assets using Unreal AssetTools.
- Dual-alias parameter parsing (snake_case and PascalCase).
- Error handling and JSON response structure (`FAgentFrameworkActionResult` / `FAgentFrameworkActionUtils`).

Write your findings and implementation recommendation to `c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\explorer_m4_code\analysis.md` and send a message back with your report.
