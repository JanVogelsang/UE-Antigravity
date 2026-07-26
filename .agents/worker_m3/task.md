# Task Description — Worker (Milestone 3: MetaSound Actions)

Working Directory: `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/worker_m3`

## MANDATORY INTEGRITY WARNING
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

## Objective
Implement Milestone 3 (MetaSound Actions Specs 6 & 7):
1. Add `MetasoundEditor` to `PrivateDependencyModuleNames` in `AgentFramework/Source/AgentFrameworkActions/AgentFrameworkActions.Build.cs`.
2. Create `FAgentFrameworkMetaSoundActions` header and implementation:
   - `AgentFramework/Source/AgentFrameworkActions/Public/MetaSound/AgentFrameworkMetaSoundActions.h`
   - `AgentFramework/Source/AgentFrameworkActions/Private/MetaSound/AgentFrameworkMetaSoundActions.cpp`
   - Implement `create_metasound_source` (Spec 6)
   - Implement `wire_metasound_nodes` (Spec 7)
3. Register `FAgentFrameworkMetaSoundActions` in `AgentFramework/Source/AgentFramework/Private/Server/AgentFrameworkHttpServer.cpp` (`InRouter->RegisterExecutor(MakeShared<FAgentFrameworkMetaSoundActions>());`).
4. Create schema file `AgentFramework/Resources/ToolSchemas/metasound_tools.json`.

## Reference Inputs
Read the analysis and handoff report from `explorer_m3_1`:
- `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/explorer_m3_1/analysis.md`
- `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/explorer_m3_1/handoff.md`
- `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/Documentation/PYTHON_FALLBACK_AUDIT.md` (Section 4, Specs 6 & 7)

## Output Requirements
- Output summary of changes to `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/worker_m3/changes.md`.
- Output handoff report to `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/worker_m3/handoff.md`.
