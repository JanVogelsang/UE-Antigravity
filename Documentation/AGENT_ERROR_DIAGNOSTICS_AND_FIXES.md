# Agent Error Diagnostics & Remediation Plan

This document details the forensic log analysis of agent execution errors, Unreal Engine crashes, editor popups, and friction points observed during the advanced benchmark runs. Future agents assigned to resolve these issues should use this report as an empirical guide.

---

## 1. Primary Log & Transcript Evidence Index

All findings in this assessment are grounded in the following log files and subagent trajectory transcripts:

### Unreal Engine Editor Logs
* **Primary Assertion & Crash Log:**  
  [`AgentFrameworkTest-backup-2026.07.28-12.41.07.log`](file:///C:/Users/janv1/Documents/Unreal%20Projects/AgentFrameworkTest/Saved/Logs/AgentFrameworkTest-backup-2026.07.28-12.41.07.log)  
  *Contains assertion crashes in `Kismet2.cpp`, SEH exceptions in `ActionRouter`, and package creation warnings.*
* **Live Coding Lock Log:**  
  [`AgentFrameworkTest-backup-2026.07.29-11.14.34.log`](file:///C:/Users/janv1/Documents/Unreal%20Projects/AgentFrameworkTest/Saved/Logs/AgentFrameworkTest-backup-2026.07.29-11.14.34.log)  
  *Contains Live Coding compile lockouts and full UBT fallback events.*
* **Active Session Log:**  
  [`AgentFrameworkTest.log`](file:///C:/Users/janv1/Documents/Unreal%20Projects/AgentFrameworkTest/Saved/Logs/AgentFrameworkTest.log)  
  *Current active editor log.*

### Benchmark Subagent Trajectory Transcripts
* **Task 1 (`adv_task_01` - Interactive Trap):**  
  [`transcript.jsonl`](file:///C:/Users/janv1/.gemini/antigravity/brain/883272f1-27e7-47e6-b44f-c19ce73c5c4e/.system_generated/logs/transcript.jsonl)
* **Task 2 (`adv_task_02` - UMG Health Bar):**  
  [`transcript.jsonl`](file:///C:/Users/janv1/.gemini/antigravity/brain/c1427aaa-c0ce-45ca-93b1-58342ac22312/.system_generated/logs/transcript.jsonl)
* **Task 3 (`adv_task_03` - C++ Base Character & Input):**  
  [`transcript.jsonl`](file:///C:/Users/janv1/.gemini/antigravity/brain/abda208b-ecfa-4cc1-9a8b-4029401334d8/.system_generated/logs/transcript.jsonl)
* **Task 4 (`adv_task_04` - Niagara Spawner Lifecycle):**  
  [`transcript.jsonl`](file:///C:/Users/janv1/.gemini/antigravity/brain/699598a6-e38f-4c85-bf5b-be27f9266307/.system_generated/logs/transcript.jsonl)
* **Task 5 (`adv_task_05` - Data Assets & BP Interface):**  
  [`transcript.jsonl`](file:///C:/Users/janv1/.gemini/antigravity/brain/b033c301-a1e3-4b3a-8e91-6a6082fd0589/.system_generated/logs/transcript.jsonl)
* **Plugin Edit Loop Subagent (`db7e78e4`):**  
  [`transcript.jsonl`](file:///C:/Users/janv1/.gemini/antigravity/brain/db7e78e4-a83b-418a-bca7-678060e1fd80/.system_generated/logs/transcript.jsonl)  
  *Shows subagent getting stuck trying to modify plugin C++ source code when package creation failed.*

---

## 2. Categorized Error Analysis & Root Causes

### Category A: Engine Assertion Crash in `create_blueprint_actor`
* **Log Lines:** Lines 2880–2889 in `AgentFrameworkTest-backup-2026.07.28-12.41.07.log`:
  ```text
  LogWindows: Error: appError called: Assertion failed: BpCdo->HasAnyFlags(RF_ClassDefaultObject) && BlueprintClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint) [File:D:\build\++UE5\Sync\Engine\Source\Editor\UnrealEd\Private\Kismet2\Kismet2.cpp] [Line: 834] 
  LogAgentFramework: Error: ActionRouter: Intercepted SEH exception 0x00004000 during tool 'create_blueprint_actor'
  ```
* **Root Cause:** In [`AgentFrameworkBlueprintActions.cpp`](file:///c:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/AgentFramework/Source/AgentFrameworkActions/Private/Blueprint/AgentFrameworkBlueprintActions.cpp#L1068), `ExecuteCreateBlueprint` calls `FKismetEditorUtilities::CreateBlueprint`. If a Blueprint package already exists in memory or on disk at that package path, calling `CreateBlueprint` on top of an existing memory package triggers an assertion failure inside Unreal Engine's `Kismet2.cpp`.
* **Actionable Fix:** Before calling `FKismetEditorUtilities::CreateBlueprint`, check if `UEditorAssetLibrary::DoesAssetExist(PackagePath)`. If it exists, call `UEditorAssetLibrary::DeleteAsset` or load the existing `UBlueprint` object instead of calling `CreateBlueprint` on a dirty package.

---

### Category B: Package Name Split & Short Package Name Warning
* **Log Lines:** Lines 2870–2881 in `AgentFrameworkTest-backup-2026.07.28-12.41.07.log`:
  ```text
  LogUObjectGlobals: Warning: Attempted to create a package with a short package name: /Game/BenchAdv/Traps/BP_Trap.BP_Trap Outer: Package /Game/BenchAdv/Traps/BP_Trap
  LogStreaming: Warning: LoadPackage: SkipPackage: /Game/BenchAdv/Traps/BP_Trap - The package to load does not exist on disk or in the loader
  ```
* **Root Cause:** When an agent passes an object path (e.g. `/Game/BenchAdv/Traps/BP_Trap.BP_Trap`) into asset creation tools, `FPackageName::GetLongPackagePath` and `FPackageName::GetShortName` fail to split package paths containing dots (`.`), passing the full string into `CreatePackage()`.
* **Actionable Fix:** In [`AgentFrameworkActionUtils.cpp`](file:///c:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/AgentFramework/Source/AgentFrameworkActions/Private/AgentFrameworkActionUtils.cpp#L478), enforce strict splitting: extract package path (before `.`) and asset name (after `.`) before calling `CreatePackage()`.

---

### Category C: Live Coding Structural Lockout
* **Log Lines:** `AgentFrameworkTest-backup-2026.07.29-11.14.34.log`:
  ```text
  Live Coding compilation is already in progress. Please wait for it to complete.
  ```
* **Root Cause:** Creating new C++ classes with `UCLASS()` / `UPROPERTY()` macros via `macro_create_cpp_class` and triggering `trigger_compile` immediately causes a race condition if UHT (UnrealHeaderTool) is still re-indexing disk source files.
* **Actionable Fix:** In [`AgentFrameworkCppActions.cpp`](file:///c:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/AgentFramework/Source/AgentFrameworkActions/Private/Cpp/AgentFrameworkCppActions.cpp#L370), verify `ILiveCodingModule::Get().IsCompiling()` is false and trigger a synchronous file system flush before invoking `LiveCoding->Compile()`.

---

### Category D: Agent Error Blindness (Unreported Engine Logs)
* **Problem:** Agents primarily evaluate tool responses based on the JSON payload (`{ bSuccess: true }`). If the tool call succeeds partially but Unreal Engine emits a `LogOutputDevice` warning, an unsaved package popup, or a Blueprint compiler warning, the agent remains blind to the background error unless it calls `read_message_log`.
* **Actionable Fix:** Update [`AgentFrameworkActionRouter.cpp`](file:///c:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/AgentFramework/Source/AgentFrameworkEngine/Private/AgentFrameworkActionRouter.cpp) to capture `GWarn` and recent `FMessageLog` entries produced during tool execution and append them directly to the `warnings` array in the JSON response returned to the LLM agent.

---

## 3. Recommended Remediation Task Order for Next Agent

1. **Modify `AgentFrameworkBlueprintActions.cpp`**: Add `UEditorAssetLibrary::DoesAssetExist` check & clean package split in `ExecuteCreateBlueprint`.
2. **Modify `AgentFrameworkActionRouter.cpp`**: Capture `GWarn` / engine log warnings into the tool response payload.
3. **Modify `AgentFrameworkCppActions.cpp`**: Enhance Live Coding compilation lock checks.
4. **Rebuild & Verify**: Run `build_plugin.ps1` and execute `run_tests.ps1`.
