## 2026-07-25T11:35:06Z

You are the Level Refactoring Worker (worker_level).
Your working directory is C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\worker_level.
Your role is to refactor the Level action module in the UE-Antigravity plugin.

Follow the guidelines in C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\UnrealEngine\AGENTS.md and C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\Documentation\Refactoring_Swarm_Report\progress_summary.md.

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Your specific tasks:
1. Phase A (Technical Debt Cleanup):
   - Consolidate JSON parameter parsing in `AgentFramework/Source/AgentFrameworkActions/Private/Level/AgentFrameworkLevelActions.cpp` using static helpers from `UAgentFrameworkActionUtils` (in `AgentFrameworkActionUtils.h/cpp`). Replace raw `GetStringField`, `TryGetStringField`, `GetNumberField`, `TryGetNumberField`, `GetArrayField`, `TryGetArrayField`, etc. with safe helper methods.
   - Clean up `AgentFrameworkLevelActions.h` and `AgentFrameworkLevelActions.cpp` by deleting orphaned helper functions, unused includes, and dead code.
   - Implement strict null-checking (`IsValid()`) for all Unreal Engine object pointers (`UWorld*`, `AActor*`, `ULevel*`, `UObject*`, `UFactory*`, etc.) before dereferencing them.
2. Phase B (Expansion): Add a minor, isolated hook: when a level action completes successfully, play an editor notification sound under `#if WITH_EDITOR GEditor->PlayEditorSound(...) #endif` or trigger an action delegate callback.
3. Build and Test:
   - Run the build command: `$env:uebp_UATMutexNoWait = '1'; powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip`
   - Run the test suite command: `powershell -File .\Tests\run_tests.ps1`
   - Ensure zero compilation errors/warnings and 100% passing tests.
4. Write a handoff report at `C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\worker_level\handoff.md` detailing changes, compilation output, test results, and verified files.
5. Send a message to the parent (conversation ID: b52184b3-14c1-4ead-97a4-2e461d896e6d) when complete.
