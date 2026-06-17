# UE-Antigravity Test Infrastructure & E2E Testing Plan

This document details the test infrastructure, directory structure, execution flows, and the testing matrix for the UE-Antigravity Dual-MCP integration.

---

## 1. Architecture & Testing Topology

The automated End-to-End (E2E) test suite validates the integration of the Dual-MCP architecture, which consists of:

1. **Internal C++ MCP Server**: Runs inside the Unreal Editor process, listening on port `18777`. It handles queries and modifications to engine-side assets (Blueprints, runtime reflection, graphs).
2. **External Indexing & RAG MCP Server (Python)**: Runs as a standalone Python process. It handles C++ parsing using `libclang`, watches the filesystem for code changes, generates compiler commands, and queries the local vector database.
3. **Mock Agent Client / Test Runner**: A `pytest`-based test runner that manages the lifecycle of the editor and Python server, executing tests by communicating directly with both.

```
                  +--------------------------------+
                  |       pytest / Test Runner      |
                  +---------------+----------------+
                                  |
            JSON-RPC (stdio)      |      HTTP POST /api/execute_tool (Port 18777)
          +------------------------+------------------------+
          |                                                 |
          v                                                 v
 +------------------+                             +------------------+
 | External Python  |                             | Internal C++     |
 | MCP Server       |                             | MCP Server       |
 | (AST & RAG)      |                             | (Unreal Editor)  |
 +------------------+                             +------------------+
```

---

## 2. Directory Layout

All E2E test files are located in the `Tests/` directory at the project root:

```
UE-Antigravity/
├── Tests/
│   ├── requirements.txt      # pytest, requests, and pytest-asyncio
│   ├── conftest.py           # Pytest fixtures managing Unreal Editor & Python subprocesses
│   ├── mock_client.py        # Mock Agent Client wrapping stdio (JSON-RPC) & HTTP communication
│   ├── test_e2e_integration.py # E2E Integration tests verifying tool invocation and connection
│   └── README.md             # E2E Test execution guide
├── ExternalServer/
│   └── src/
│       └── main.py           # Python MCP server entrypoint
└── TEST_INFRA.md             # This document (Test infrastructure specifications)
```

---

## 3. Setup & Prerequisites

To execute the E2E tests:
1. Ensure Unreal Engine 5.7 is installed at `D:\UE_5.7` (with `UnrealEditor-Cmd.exe` located at `D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe`).
2. The target game project must be located at `c:\Users\Jan\Documents\Unreal Projects\Tau.uproject`.
3. Install Python 3.10+ and install dependencies:
   ```bash
   pip install -r Tests/requirements.txt
   ```
4. Run the test suite:
   ```bash
   pytest -v
   ```

*Note: If the Unreal Editor is already open and listening on port 18777, the testing harness will automatically detect and run against it instead of launching a new headless instance.*

---

## 4. Feature Matrix

The test infrastructure covers the following **7 Core Features**:

*   **F1: Blueprint structural schema retrieval** (`get_blueprint_schema`)
    *   *Description:* Extract Blueprint structural schema (variables, types, events, parent classes) without loading the asset memory-heavy in the editor.
*   **F2: T3D blueprint graph injection** (`inject_blueprint_nodes_t3d`)
    *   *Description:* Programmatically inject visual scripting nodes using Text-3D (T3D) payloads directly into specified graphs.
*   **F3: Runtime UHT reflection data extraction** (`get_cpp_reflection_info`)
    *   *Description:* Query metadata from UCLASS, UPROPERTY, and UFUNCTION structures reflected by the Unreal Header Tool (UHT) at runtime.
*   **F4: External C++ AST query** (`query_cpp_ast`)
    *   *Description:* Analyze game source files via Clang AST representation to locate class details, signatures, and transitive dependencies.
*   **F5: Automated compilation database update** (`generate_compile_commands`)
    *   *Description:* Automatically generate or regenerate the project's `compile_commands.json` database via Unreal Build Tool (UBT).
*   **F6: Incremental AST update filesystem watcher**
    *   *Description:* Watch filesystem directories for saves of C++ files, instantly updating the local AST cache.
*   **F7: Semantic Vector DB document search** (`search_vector_db`)
    *   *Description:* Query documentation via a localized vector database to find relevant UE5 documentation snippets.

---

## 5. Coverage Thresholds & Test Cases (Total: 82 Cases)

### Tier 1: Feature Coverage (5 cases per feature = 35 total)

*   **F1: Blueprint Schema Retrieval**
    1.  *TC-F1-01:* Retrieve schema for simple Actor Blueprint.
    2.  *TC-F1-02:* Retrieve schema for Character Blueprint containing custom events.
    3.  *TC-F1-03:* Retrieve schema for Blueprint subclassing another Blueprint.
    4.  *TC-F1-04:* Verify variables, types, and categories are correctly extracted.
    5.  *TC-F1-05:* Verify parent class path is correct.
*   **F2: T3D Graph Injection**
    6.  *TC-F2-01:* Inject single standard call node into a graph.
    7.  *TC-F2-02:* Inject custom event node.
    8.  *TC-F2-03:* Inject variable getter/setter node.
    9.  *TC-F2-04:* Inject connected sequence of nodes.
    10. *TC-F2-05:* Inject nodes and verify compilation succeeds.
*   **F3: UHT Reflection Data Extraction**
    11. *TC-F3-01:* Retrieve reflection data for standard UCLASS.
    12. *TC-F3-02:* Retrieve properties marked UPROPERTY (types, categories).
    13. *TC-F3-03:* Retrieve functions marked UFUNCTION (arguments, return type).
    14. *TC-F3-04:* Extract metadata tags (e.g. Category, DisplayName).
    15. *TC-F3-05:* Retrieve interfaces implemented by a class.
*   **F4: External C++ AST Query**
    16. *TC-F4-01:* Query class declaration and find inheritance chain.
    17. *TC-F4-02:* Query method signature and parameters.
    18. *TC-F4-03:* Query transitively called functions within a method body.
    19. *TC-F4-04:* Query UPROPERTY/UFUNCTION declarations.
    20. *TC-F4-05:* Query symbol usages in a compilation unit.
*   **F5: Compilation Database Update**
    21. *TC-F5-01:* Trigger UBT Clang Database generator and verify `compile_commands.json` is generated.
    22. *TC-F5-02:* Verify JSON schema of `compile_commands.json`.
    23. *TC-F5-03:* Verify all source files in the project are listed in database.
    24. *TC-F5-04:* Verify compile arguments contain the correct include paths.
    25. *TC-F5-05:* Verify generated database matches current build targets.
*   **F6: Incremental Watcher**
    26. *TC-F6-01:* Modify a C++ file and verify watcher detects change.
    27. *TC-F6-02:* Verify watcher triggers `libclang` parsing of modified file.
    28. *TC-F6-03:* Verify AST cache updates within 3-second threshold.
    29. *TC-F6-04:* Verify watcher ignores files in excluded directories (e.g. `Binaries/`, `Intermediate/`).
    30. *TC-F6-05:* Verify watcher handles file deletion by removing symbols from SQLite cache.
*   **F7: Vector DB Search**
    31. *TC-F7-01:* Perform query and retrieve matching document snippets.
    32. *TC-F7-02:* Verify similarity score thresholds.
    33. *TC-F7-03:* Verify results contain proper source file links.
    34. *TC-F7-04:* Verify search handles multi-word query parameters.
    35. *TC-F7-05:* Verify pagination of search results.

### Tier 2: Boundary & Corner Cases (5 cases per feature = 35 total)

*   **F1: Blueprint Schema Retrieval**
    36. *TC-F1-06:* Query non-existent Blueprint asset path (assert correct error code).
    37. *TC-F1-07:* Query invalid/corrupt Blueprint asset.
    38. *TC-F1-08:* Query empty Blueprint with no variables or events.
    39. *TC-F1-09:* Query Blueprint with circular inheritance dependencies.
    40. *TC-F1-10:* Query Blueprint containing complex map/set structures.
*   **F2: T3D Graph Injection**
    41. *TC-F2-06:* Inject malformed/broken T3D payload (verify validation fails gracefully).
    42. *TC-F2-07:* Inject nodes into non-existent graph.
    43. *TC-F2-08:* Inject duplicate node IDs (verify resolving mechanism).
    44. *TC-F2-09:* Inject nodes when PIE (Play-In-Editor) is active (verify warning/blocking).
    45. *TC-F2-10:* Inject nodes with missing class/reference dependencies.
*   **F3: UHT Reflection Data Extraction**
    46. *TC-F3-06:* Query non-existent UCLASS name (verify clear error message).
    47. *TC-F3-07:* Query non-UObject C++ struct or namespace.
    48. *TC-F3-08:* Query class containing private/protected reflectable members.
    49. *TC-F3-09:* Query class with heavy macro-based generic templates.
    50. *TC-F3-10:* Query class loaded dynamically from runtime-loaded plugin modules.
*   **F4: External C++ AST Query**
    51. *TC-F4-06:* Query AST on unsaved or dirty file.
    52. *TC-F4-07:* Query AST on syntactically invalid C++ file (verify fallback parser behavior).
    53. *TC-F4-08:* Query file containing circular includes or heavy preprocessor macros.
    54. *TC-F4-09:* Query non-existent file path.
    55. *TC-F4-10:* Query huge file (>10k LOC) and verify execution time is within bounds.
*   **F5: Compilation Database Update**
    56. *TC-F5-06:* Trigger generator with compile errors in project files (verify DB still updates).
    57. *TC-F5-07:* Trigger generator with missing `.uproject` dependencies.
    58. *TC-F5-08:* Trigger generator concurrently from multiple threads/clients (verify locking).
    59. *TC-F5-09:* Trigger generator with read-only database file permissions.
    60. *TC-F5-10:* Verify cleanup of temporary build generator files.
*   **F6: Incremental Watcher**
    61. *TC-F6-06:* Modify multiple files in rapid succession (verify debouncing).
    62. *TC-F6-07:* Rename/Move a file (verify removal of old path and index of new path).
    63. *TC-F6-08:* Save file with no modifications (verify index skip).
    64. *TC-F6-09:* File lock conflicts during edit (verify retry mechanism).
    65. *TC-F6-10:* Watcher initialization on deeply nested workspace folders.
*   **F7: Vector DB Search**
    66. *TC-F7-06:* Search with empty string/None query.
    67. *TC-F7-07:* Search with special/non-ASCII characters.
    68. *TC-F7-08:* Search with extremely long query text.
    69. *TC-F7-09:* Verify behavior when vector database is empty or uninitialized.
    70. *TC-F7-10:* Query with nonexistent keywords (verify low similarity score handling).

### Tier 3: Cross-Feature Combinations (7 cases)

71. *TC-F3-C01 (Compilation DB & AST Query):* Python server triggers compilation database update, parses the updated project via watcher, and successfully processes an AST query on a new class.
72. *TC-F3-C02 (UHT Reflection & Blueprint Schema):* Query reflection info of a C++ base class via `get_cpp_reflection_info`, then query a Blueprint subclass schema via `get_blueprint_schema` to verify UPROPERTY overrides/inheritance.
73. *TC-F3-C03 (AST Query & Blueprint Injection):* Query C++ AST for method signature, format corresponding T3D nodes, and inject them into a Blueprint graph.
74. *TC-F3-C04 (Vector DB Search & C++ AST):* Search documentation for a class/API, then use `query_cpp_ast` to locate its local source implementations.
75. *TC-F3-C05 (AST Update Watcher & UHT Reflection):* Modify a C++ header containing UHT macros, watch it update in the SQLite cache, and verify runtime reflection update via `get_cpp_reflection_info`.
76. *TC-F3-C06 (Unified Generation & Injection):* Automatically trigger compilation database update, retrieve Blueprint structural schema, and inject nodes linking the C++ symbols to the Blueprint graph.
77. *TC-F3-C07 (Error Cascading Flow):* Inject invalid nodes (F2), fail, query AST (F4) to recover signature, search documentation (F7) for correct usage, and successfully re-inject.

### Tier 4: Real-World Application Scenarios (5 cases)

78. *TC-F4-S01 (New Character Class Implementation):* Create new C++ class, wait for file watcher to parse AST, generate compile commands, compile via Unreal Editor, and verify UHT reflection details.
79. *TC-F4-S02 (Dynamic UI Event Wiring):* Extract structural schema from user widget Blueprint, query C++ AST for click handler signature, generate T3D delegate nodes, and inject them.
80. *TC-F4-S03 (Gameplay Ability System Hookup):* Query vector DB for GAS setup, query UHT reflection info for attribute sets, retrieve Blueprint schema for target Gameplay Ability, and inject required activation nodes.
81. *TC-F4-S04 (Massive Codebase Sync):* Bulk-modify multiple headers/source files, verify file watcher handles incremental indexing queue without memory bloat, and perform concurrent AST queries.
82. *TC-F4-S05 (Headless CI Pipeline Verification):* Run full test execution on a clean virtual environment, starting Unreal headlessly, running all unit/integration tests, and shutting down cleanly.
