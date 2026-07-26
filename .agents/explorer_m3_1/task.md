# Task Description — Explorer (Milestone 3: MetaSound Actions)

Working Directory: `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/explorer_m3_1`

## Objective
Investigate `Documentation/PYTHON_FALLBACK_AUDIT.md` Spec 6 (`create_metasound_source`) and Spec 7 (`wire_metasound_nodes`).
Investigate the requirements to create a NEW action executor class `FAgentFrameworkMetaSoundActions`:
- Header: `AgentFrameworkActions/Public/MetaSound/AgentFrameworkMetaSoundActions.h`
- Source: `AgentFrameworkActions/Private/MetaSound/AgentFrameworkMetaSoundActions.cpp`
- Module dependencies: `AgentFrameworkActions.Build.cs` (add `MetasoundEngine`, `MetasoundFrontend`, `MetasoundEditor`)
- Registration: `AgentFramework/Private/Server/AgentFrameworkHttpServer.cpp` (instantiate and register `FAgentFrameworkMetaSoundActions` routes)

## Required Analysis
- Analyze MetaSound Frontend & Editor C++ APIs (`UMetaSoundSource`, `Metasound::Frontend::...`, `IMetaSoundDocumentBuilder`, builder interface or graph modification utilities).
- Map out `create_metasound_source` parameters (`destination_path`, `asset_name`, `num_channels`, `is_preset`, `preset_source_path`).
- Map out `wire_metasound_nodes` parameters (`asset_path`, `connections`, node lookup, handle wiring).
- Map out `AgentFrameworkActions.Build.cs` changes and `AgentFrameworkHttpServer.cpp` route registration structure.
- Write `analysis.md` and `handoff.md` in `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/explorer_m3_1/`.
