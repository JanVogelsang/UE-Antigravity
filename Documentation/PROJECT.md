# Project: UE-Antigravity Dual-MCP Architecture

## Architecture
The system consists of two primary servers:
1. **Internal C++ MCP Server**: Runs inside the Unreal Engine Editor process. It listens on HTTP port 18777 (already partially defined via `FAntigravityHttpServer`) and accepts requests representing MCP-like actions. The stdio bridge `bridge.exe` translates standard input/output (stdio) MCP JSON-RPC protocol messages to loopback HTTP requests to the editor.
2. **External Indexing & RAG MCP Server (Python)**: Runs as a standalone Python process. It exposes tools to analyze C++ source files using `libclang`, generate `compile_commands.json` using UBT, watch files for modification to incrementally update an SQLite AST cache, and perform vector database semantic searches.
3. **Mock Agent Client / Test Runner**: A testing component that acts as the client connecting to both MCP servers to verify end-to-end integration.

## Code Layout
- `Antigravity/` - Unreal Engine editor plugin containing C++ source and resources.
  - `Source/AntigravityActions/` - Main plugin implementation containing actions/tools.
  - `Source/AntigravityActions/Private/Diagnostics/` - Automated unit/integration tests for C++ side.
- `UnrealEngine/` - The C++ bridge source and client profile configuration.
- `ExternalServer/` - The External Python Indexing & RAG server.
  - `src/` - Python server implementation (`mcp` library stdio server).
  - `tests/` - Python unit/integration tests (pytest).
- `Tests/` - Workspace integration tests and mock agent client.
- `run_all_tests.ps1` - PowerShell script to trigger the entire test suite headlessly (both C++ and Python).

## Milestones
| # | Name | Scope | Dependencies | Status | Conversation ID |
|---|------|-------|-------------|--------|-----------------|
| 1 | C++ Blueprint Schema Tool | Implement asset-registry-based Blueprint `.uasset` structural schema extraction without loading object | None | DONE | 6ea4e77e-43c9-4698-be5a-a4d48d645ff9 |
| 2 | C++ T3D Injection & UHT Metadata | Validate T3D Node GUID injection and implement UHT reflection data extraction at runtime | M1 | IN_PROGRESS | aa637e1d-1edf-40c8-aa6e-1b9789ff504b |
| 3 | Python AST & Compilation DB | Setup python libclang AST parsing and automated `compile_commands.json` generation via UBT | None | IN_PROGRESS | 16164949-72df-4c84-85c7-2dcfaca926e6 |
| 4 | Python Watcher & Vector DB | Implement filesystem watcher for incremental AST caching and Vector DB semantic search | M3 | PAUSED | TBD |
| 5 | E2E Testing Suite | Develop E2E integration test suite, headless test script, and verify Dual-MCP communication flow | M2, M4 | PAUSED | TBD |

## Interface Contracts
### Client / Bridge ↔ Internal C++ Server
- Protocol: JSON-RPC over stdio (Bridge) mapped to HTTP POST `/api/execute_tool` (Editor).
- Standard payload:
  - Request: `{"name": "<tool_name>", "arguments": { ... }}`
  - Response: `{"bSuccess": true, "ResultMessage": "...", "Errors": [], "Warnings": []}`

### Client ↔ External Python Server
- Protocol: Standard MCP (JSON-RPC 2.0 over stdio).
- Tools exposed:
  - `query_cpp_ast`
  - `generate_compile_commands`
  - `search_vector_db`

### C++ Blueprint Schema JSON Format
```json
{
  "asset_name": "MyBlueprint",
  "parent_class": "Actor",
  "variables": [
    {"name": "MyVar", "type": "float", "category": "Default"}
  ],
  "custom_events": [
    "MyCustomEvent"
  ]
}
```
