## 2026-07-25T18:59:51Z
You are the Worker subagent (`worker_pie`) responsible for refactoring and expanding Module 19: PIE (`AgentFrameworkPIEActions`) in the `UE-Antigravity` Unreal Engine plugin.
Your working directory for metadata/handoff is `C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\worker_pie`.

## Objective
Refactor `AgentFrameworkPIEActions.h` and `AgentFrameworkPIEActions.cpp` located in `AgentFramework/Source/AgentFrameworkActions/Public/PIE/` and `Private/PIE/`.

## Tasks
1. **Phase A: Technical Debt Cleanup**
   - Consolidate all raw JSON parameter parsing boilerplate into `UAgentFrameworkActionUtils` helper functions (`TryGetStringParam`, `TryGetBoolParam`, `TryGetIntParam`, `TryGetStringArrayParam`, etc.).
   - Implement strict null-checking using Unreal's `IsValid()` macro for all `UObject`, `AActor`, `UWorld`, `UGameInstance`, `APlayerController`, `APawn`, etc., pointers before accessing them. Guard `GEditor` calls with `if (GEditor)`.
   - Remove orphaned helper functions, unused `#include` directives, and dead/commented-out code.

2. **Phase B: Targeting Missing Hooks (Expansion)**
   - Implement minor, isolated missing hooks adjacent to the PIE module:
     - Add `#if WITH_EDITOR` preprocessor guards around editor sound playback (`GEditor->PlayEditorSound(SuccessSound)`) when PIE actions execute successfully, loading/verifying `SuccessSound` with `IsValid()`.

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
