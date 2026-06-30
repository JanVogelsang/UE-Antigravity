---
name: blueprint-authoring
description: SOPs, T3D rules, and sub-agent workflows for creating and modifying Unreal Engine Blueprints.
---
# Skill: Blueprint Authoring

This skill defines the workflows, rules, and procedures for AI agents to create and modify Unreal Engine Blueprint assets using T3D injection.

## SOP: Modifying a Blueprint Graph

When modifying or creating a Blueprint Graph, you MUST follow this sequence:

1.  **Step 1: Check Current State**: Use `get_blueprint_info` with `exclude_visual_layout=true` or `query_mode="interface_only"` to fetch the current graph interface without coordinates, saving massive tokens. Cache the returned `client_hash` and pass it in subsequent calls to return lightweight `up_to_date=true` results if unchanged.
2.  **Step 2: Batch Structural Changes**: Use `execute_batch_blueprint_operations` to batch multiple graph modifications (components, variables, functions) in a single transaction, compiling once at the end.
3.  **Step 3: Draft and Format T3D Nodes**: Draft the T3D nodes. To avoid node overlaps, you **MUST** call the `format_t3d_layout` tool on the drafted T3D text to calculate clean coordinates. Then, use `inject_blueprint_nodes_t3d` with the formatted T3D text and the inline `connections` array parameter to import nodes and wire them to existing nodes in a single tool call.
    *   **CRITICAL**: When generating T3D (plain-text) representation for nodes (for `inject_blueprint_nodes_t3d` or copy-pasting), you **MUST** include a unique, valid `NodeGuid` parameter (a 32-character uppercase hexadecimal string, e.g., `NodeGuid=3E2A5D8446B84A29B52C2D812A2BD5F5`) for every single node. If omitted or duplicated, the imported nodes will lack a unique GUID, causing "missing NodeGuid" warnings during cooking.
4.  **Step 4: Connect Isolated Pins**: If isolated wiring is needed later, use `connect_blueprint_pins`. Compile step runs automatically on modifications.
5.  **Step 5: Mandatory Post-Injection Audit**: After every injection, read the entire result message. If a `PINS REQUIRING ATTENTION` or `SANITISER` warning section is present, act on EVERY entry before considering the task done. Never leave asset references empty or numeric defaults at zero unless explicitly requested.

## T3D Placeholder Substitution
When replacing placeholders (e.g. `LINK_1`, `LINK_10`) in T3D node definitions, sort placeholders by length descending before replacement to prevent prefix collisions (e.g. replacing `LINK_10` with the value of `LINK_1` + `0`).

## Sub-Agent Workflow for Blueprint Authoring (Antigravity 4-Layer Context)

When complex authoring is required, the Main Agent coordinates three specialized sub-agents:

### 1. Planner Sub-agent (Role: `Blueprint Architect`)
*   **Responsibility**: Decomposes the user's high-level goal into step-by-step T3D graph injection actions.
*   **Tools Allowed**: `search_similar_blueprints` (Layer 4 context), `get_blueprint_schema` (Layer 2), `search_assets`, read-only queries.
*   **MANDATORY PRE-FLIGHT GAP ANALYSIS:**
    *   Before producing any plan, the Architect **MUST** call `get_blueprint_info` on the target Blueprint and fetch relevant Blueprint schema.
    *   The Architect **MUST** generate a `<GAP_ANALYSIS>` block explicitly comparing the existing nodes, variables, and pins with the user's requested goal.
    *   The Architect must detail what nodes are missing, which pins must be wired, and how the changes map to the target graph.

### 2. Executor Sub-agent (Role: `Blueprint Engineer`)
*   **Responsibility**: Translates the plan into actual `inject_blueprint_nodes_t3d` or `execute_batch_blueprint_operations` commands.
*   **Tools Allowed**: Modifying tools (Layer 1 manipulation), `format_t3d_layout`.
*   **Workflow**: 
    1.  Consolidates edits and drafts the raw T3D node text.
    2.  **MANDATORY BEAUTIFICATION PASS:** To prevent overlapping nodes, the Engineer **MUST** call the `format_t3d_layout` tool on the drafted T3D text *before* injection.
    3.  Uses the formatted T3D with `execute_batch_blueprint_operations` or `inject_blueprint_nodes_t3d` to run in a single transaction.
    4.  Audits pin states and reports completion.

### 3. Verification Sub-agent (Role: `QA Auditor`)
*   **Responsibility**: Checks the work after the transaction.
*   **Tools Allowed**: `compile_blueprint`, `run_automation_tests`.
*   **Workflow**: Compiles the Blueprint. If it fails, instructs the Executor to fix it via another transaction or explicitly undo it. If successful, completes the task.

**Routing Protocol**: The Main Agent MUST NOT execute complex Blueprint logic directly. It must delegate to the Architect, pass the plan to the Engineer, and then have the QA Auditor verify and commit.
