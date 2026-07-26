# Task Description — Forensic Auditor (Milestone 4: Integrity Verification)

Working Directory: `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/auditor_m4`

## Objective
Perform independent forensic integrity verification of all C++ changes introduced for Phase 2 (Tier 1) native C++ action routes:
- `AgentFrameworkActions/Public/Blueprint/AgentFrameworkBlueprintActions.h` & `Private/Blueprint/AgentFrameworkBlueprintActions.cpp`
- `AgentFrameworkActions/Public/Material/AgentFrameworkMaterialActions.h` & `Private/Material/AgentFrameworkMaterialActions.cpp`
- `AgentFrameworkActions/Public/MetaSound/AgentFrameworkMetaSoundActions.h` & `Private/MetaSound/AgentFrameworkMetaSoundActions.cpp`
- `AgentFrameworkActions.Build.cs`
- `AgentFrameworkHttpServer.cpp`

## Integrity Audit Checks
1. Check for hardcoded test outputs or return values in any action executor.
2. Check for stub, facade, or dummy implementations.
3. Confirm genuine usage of Unreal Engine C++ APIs (`UBlueprint`, `FBlueprintEditorUtils`, `FKismetEditorUtilities`, `UMaterial`, `UMaterialExpressionTextureSampleParameter2D`, `UMetaSoundSource`, `IMetaSoundDocumentBuilder`).
4. Output forensic audit report to `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/auditor_m4/audit.md` and handoff report to `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/auditor_m4/handoff.md`. Declare verdict as CLEAN or INTEGRITY VIOLATION.
