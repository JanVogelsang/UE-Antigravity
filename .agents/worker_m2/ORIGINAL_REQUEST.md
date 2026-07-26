## 2026-07-26T15:17:35Z
Your working directory is: c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/worker_m2

DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Task: Implement Milestone 2 of Phase 2 UE-AgentFramework Roadmap:
Implement 3 new native C++ action routes in AgentFrameworkActions for Diagnostics and Context modules, and update their respective JSON schemas:

1. `find_unreferenced_assets` (Spec 13 from Documentation/PYTHON_FALLBACK_AUDIT.md)
   - Header: `AgentFramework/Source/AgentFrameworkActions/Public/Diagnostics/AgentFrameworkDiagnosticsActions.h`
   - Implementation: `AgentFramework/Source/AgentFrameworkActions/Private/Diagnostics/AgentFrameworkDiagnosticsActions.cpp`
   - Schema: `AgentFramework/Resources/ToolSchemas/diagnostics_tools.json`
   - Details:
     - Register action `find_unreferenced_assets` in `FAgentFrameworkDiagnosticsActions`.
     - Parameters: `folder_path` (string, required), `include_soft_references` (boolean, default true).
     - Logic: Query `IAssetRegistry` for all package names under `folder_path`. Call `GetReferencers()` for each package name. Filter packages that have zero external referencers. Return JSON with `unreferenced_assets` array, `count`, and status.

2. `inspect_uobject_properties` (Spec 15 from Documentation/PYTHON_FALLBACK_AUDIT.md)
   - Header: `AgentFramework/Source/AgentFrameworkActions/Public/Diagnostics/AgentFrameworkDiagnosticsActions.h`
   - Implementation: `AgentFramework/Source/AgentFrameworkActions/Private/Diagnostics/AgentFrameworkDiagnosticsActions.cpp`
   - Schema: `AgentFramework/Resources/ToolSchemas/diagnostics_tools.json`
   - Details:
     - Register action `inspect_uobject_properties` in `FAgentFrameworkDiagnosticsActions`.
     - Parameters: `object_path` (string, required), `include_inherited` (boolean, default true).
     - Logic: Load target `UObject` via `StaticLoadObject(UObject::StaticClass(), nullptr, *object_path)`. Iterate property fields via `TFieldIterator<FProperty>(TargetObject->GetClass())`. Serialize property names and exported text values into a JSON object. Return JSON response.

3. `consolidate_asset_references` (Spec 11 from Documentation/PYTHON_FALLBACK_AUDIT.md)
   - Header: `AgentFramework/Source/AgentFrameworkActions/Public/Context/AgentFrameworkContextActions.h`
   - Implementation: `AgentFramework/Source/AgentFrameworkActions/Private/Context/AgentFrameworkContextActions.cpp`
   - Schema: `AgentFramework/Resources/ToolSchemas/context_tools.json`
   - Details:
     - Register action `consolidate_asset_references` in `FAgentFrameworkContextActions`.
     - Parameters: `source_asset_path` (string, required), `target_asset_path` (string, required).
     - Logic: Load source and target `UObject` pointers using `StaticLoadObject`. Invoke `UEditorAssetLibrary::ConsolidateAssets(TargetAsset, { SourceAsset })` or `ObjectTools::ConsolidateObjects`. Return JSON response.

Update `diagnostics_tools.json` and `context_tools.json` in `AgentFramework/Resources/ToolSchemas/` to include the tool definitions and input schemas for these 3 new tools.

When done, write a comprehensive report in `.agents/worker_m2/handoff.md` summarizing files modified, C++ methods implemented, JSON schemas added, and verification performed.
