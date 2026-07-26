# Task Description — Worker (Milestone 1: Blueprint Actions)

Working Directory: `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/worker_m1`

## MANDATORY INTEGRITY WARNING
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

## Objective
Implement 4 new native C++ action tools in `FAgentFrameworkBlueprintActions`:
1. `disconnect_blueprint_pins` (Spec 1 in `Documentation/PYTHON_FALLBACK_AUDIT.md`)
2. `modify_blueprint_subobject` (Spec 2 in `Documentation/PYTHON_FALLBACK_AUDIT.md`)
3. `configure_actor_replication` (Spec 3 in `Documentation/PYTHON_FALLBACK_AUDIT.md`)
4. `set_variable_replication` (Spec 4 in `Documentation/PYTHON_FALLBACK_AUDIT.md`)

## Reference Inputs
Read the analysis and handoff reports from the 3 Explorers:
- `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/explorer_m1_1/analysis.md`
- `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/explorer_m1_2/analysis.md`
- `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/explorer_m1_3/analysis.md`
- `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/Documentation/PYTHON_FALLBACK_AUDIT.md` (Section 4, Specs 1-4)

## Source Files to Modify
- `AgentFramework/Source/AgentFrameworkActions/Public/Blueprint/AgentFrameworkBlueprintActions.h`
- `AgentFramework/Source/AgentFrameworkActions/Private/Blueprint/AgentFrameworkBlueprintActions.cpp`

## Execution Requirements
- Implement full, robust C++ methods for the 4 tools.
- Add necessary headers (`Kismet2/BlueprintEditorUtils.h`, `Kismet2/KismetEditorUtilities.h`, `Engine/Blueprint.h`, `Engine/SimpleConstructionScript.h`, `Engine/SCS_Node.h`, etc.).
- Ensure proper error handling, JSON response formatting, `FBlueprintEditorUtils::MarkBlueprintAsModified`, `FKismetEditorUtilities::CompileBlueprint`, and dirty package tracking.
- Output changes summary to `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/worker_m1/changes.md` and handoff report to `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/worker_m1/handoff.md`.
