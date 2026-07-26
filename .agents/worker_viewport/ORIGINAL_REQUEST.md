## 2026-07-25T18:39:36Z
You are the Worker subagent for Module 26 (Viewport / AgentFrameworkViewportActions) in the UE-Antigravity Unreal Engine plugin refactoring project.

Working directory: `C:\Users\janv1\Documents\Unreal Projects\UE-Antigravity`
Agent folder: `.agents/worker_viewport/`

### Mission & Tasks:
1. **Phase A (Technical Debt Cleanup)**:
   - Examine `AgentFrameworkViewportActions` (and related files in `AgentFramework/Source/AgentFrameworkActions/Private/Viewport/` and `Public/Viewport/`).
   - Consolidate all JSON parameter extraction boilerplate to use standard helpers in `UAgentFrameworkActionUtils` (e.g., `TryGetStringParam`, `TryGetBoolParam`, `TryGetFloatParam`, `TryGetObjectParam`, etc.).
   - Delete unused includes, dead code, and orphaned helper functions.
   - Enforce strict null-checking using `IsValid()` for all Unreal objects, and check pointer validity before accessing viewports/viewport clients.

2. **Phase B (Hook Expansion)**:
   - Implement minor, isolated missing hooks adjacent to the `Viewport` module (e.g., viewport camera transform/speed adjustment, view mode toggle, real-time rendering toggle, or viewport focus on selection) to expand capabilities cleanly.

3. **Compilation Verification**:
   - Run the build script `powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip` (or equivalent UBT command) from the workspace root.
   - Verify that the target module compiles cleanly with ZERO warnings or errors.

MANDATORY INTEGRITY WARNING: DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task.

4. **Handoff**:
   - Write your handoff report to `.agents/worker_viewport/handoff.md` detailing:
     - Files modified
     - JSON consolidation changes
     - Null safety checks added
     - New hooks added
     - Build verification results
   - Send a message to the orchestrator (conversation ID: `3abb8c52-f40d-4ec2-842a-286138aded8f`) when complete.
