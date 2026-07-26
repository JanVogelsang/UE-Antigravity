## 2026-07-26T01:06:20Z
You are the Enhanced Clang AST Server Worker (teamwork_preview_worker).

Working directory for your metadata: c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\.agents\worker_ast_server

Task: Implement Milestone 4 (R4 Enhanced Clang AST Server) of the UE-AgentFramework Plugin Improvement Roadmap.

Objectives:
1. Upgrade `UnrealEngine/ExternalServer/src/main.py` to support real-time header file watch updates:
   - Add a file system watcher (`watchdog.observers.Observer` or fallback background observer thread) monitoring header (`.h`, `.hpp`) and source (`.cpp`) files.
   - Incrementally parse and re-index updated headers in real-time to refresh the SQLite AST cache (`ast_cache.db`).
2. Add Macro Expansion Inspection (`inspect_macro_expansion` / `query_macro_expansion` tool):
   - Given a macro name, file, or symbol location, extract its definition, file location, parameters, and macro expansion representation (including Unreal Engine macros like `UCLASS`, `GENERATED_BODY`, `UPROPERTY`, `UFUNCTION`, etc.).
3. Add Multi-File Call Graph Visualization (`visualize_call_graph` / `query_call_graph` tool):
   - Given a function/method symbol, perform multi-file AST traversal to catalog caller and callee nodes across files.
   - Return structured call graph data including JSON hierarchy and Mermaid graph syntax (`graph TD ...`) for visualization.
4. Expose tools cleanly via the JSON-RPC / MCP server interface in `main.py`.
5. Write an automated pytest test script (e.g. in `Tests/test_ast_enhanced.py` or `UnrealEngine/ExternalServer/tests/test_ast_features.py`) to verify real-time header watch updating, macro inspection, and call graph visualization.
6. Verify python execution and test pass (`powershell -File .\Tests\run_tests.ps1` or `pytest`).
7. Write `handoff.md` in `.agents/worker_ast_server/` and send a summary message back to the orchestrator.

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A Forensic Auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.
