## 2026-07-17T18:46:14Z
You are the DataAsset Refactoring Worker (worker_dataasset).
Your working directory is C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\worker_dataasset.
Your role is to refactor the DataAsset action module in the UE-Antigravity plugin.

Follow the instructions in C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\UnrealEngine\AGENTS.md for coding guidelines.

Your specific tasks:
1. Consolidate JSON parsing in `AgentFrameworkDataAssetActions.cpp` using static helpers from `UAgentFrameworkActionUtils` (found in `AgentFrameworkActionUtils.h/cpp`). Specifically, replace raw `GetStringField`, `TryGetStringField`, `GetNumberField`, `TryGetNumberField`, `GetArrayField`, `TryGetArrayField`, etc. with the safe helper methods where applicable, or expand `UAgentFrameworkActionUtils` with new static helpers if needed and use them.
2. Clean up `AgentFrameworkDataAssetActions.h` and `AgentFrameworkDataAssetActions.cpp` by deleting orphaned helper functions, unused includes, and dead code.
3. Implement strict null-checking (`IsValid()`) for all Unreal Engine object pointers in these files to prevent Editor crashes. (Note: Only use `IsValid` on actual Unreal Object pointers inheriting from UObject, such as UClass*, UDataAsset*, UObject*, UFactory*, etc. Do not use it on non-UObjects like FProperty* or TSharedPtr*).
4. Phase B (Expansion): Add a minor, isolated hook: when an action executes successfully, play a success notification sound using `GEditor->PlayEditorSound` (under `#if WITH_EDITOR`), or trigger a delegate callback. You can define a USoundBase* pointer or load a default sound (e.g., loading a sound asset like /Engine/EditorSounds/Notifications/CompileSuccess).
5. Compile and test: Run the build command (`$env:uebp_UATMutexNoWait = '1'; powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip`) and run the test command (`powershell -File .\Tests\run_tests.ps1`) to verify compilation and test correctness.
6. Write a handoff report at `.agents/worker_dataasset/handoff.md` detailing changes, compilation output, test results, and verified files.
7. Send a message to the parent (conversation ID: eb3fe0d9-49b5-4ba0-9bbc-0089da0e4737) when complete.
