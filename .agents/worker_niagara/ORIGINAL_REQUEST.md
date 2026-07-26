## 2026-07-25T16:39:05Z

You are the Worker subagent (`worker_niagara`) responsible for refactoring and expanding Module 17: Niagara (`AgentFrameworkNiagaraActions`) in the `UE-Antigravity` Unreal Engine plugin.
Your working directory for metadata/handoff is `C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\worker_niagara`.

## Objective
Refactor `AgentFrameworkNiagaraActions.h` and `AgentFrameworkNiagaraActions.cpp` located in `AgentFramework/Source/AgentFrameworkActions/Public/Niagara/` and `Private/Niagara/`.

## Tasks
1. **Phase A: Technical Debt Cleanup**
   - Consolidate all raw JSON parameter parsing boilerplate into `UAgentFrameworkActionUtils` helper functions (`TryGetStringParam`, `TryGetBoolParam`, `TryGetIntParam`, `TryGetStringArrayParam`, etc.).
   - Implement strict null-checking using Unreal's `IsValid()` macro for all `UObject`, `AActor`, `UNiagaraSystem`, `UNiagaraComponent`, etc., pointers before accessing them. Guard `GEditor` calls with `if (GEditor)`.
   - Remove orphaned helper functions, unused `#include` directives, and dead/commented-out code.

2. **Phase B: Targeting Missing Hooks (Expansion)**
   - Implement minor, isolated missing hooks adjacent to the Niagara module:
     - Add `#if WITH_EDITOR` preprocessor guards around editor sound playback (`GEditor->PlayEditorSound(SuccessSound)`) when Niagara actions execute successfully, loading/verifying `SuccessSound` with `IsValid()`.

3. **Build & Verification**
   - Run the plugin build script to verify compilation:
     ```powershell
     $env:uebp_UATMutexNoWait = '1'
     powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip
     ```
   - Run automated unit tests to verify zero failures:
     ```powershell
     powershell -File .\Tests\run_tests.ps1
     ```
   - Document build and test outputs in your handoff report.

## MANDATORY INTEGRITY WARNING
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

## Deliverables
Write `progress.md` and `handoff.md` in `C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\worker_niagara\`.
Send a completion message via `send_message` to parent conversation ID `de0c1035-aacb-48a9-9813-5d7846f716f8`.
