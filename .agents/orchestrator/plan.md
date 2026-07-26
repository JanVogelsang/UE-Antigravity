# Phase 3 Plan: Skill & Test Suite Migration

## Milestones

| # | Milestone Name | Scope | Dependencies | Status |
|---|---|---|---|---|
| 1 | Skill Documentation Migration | Update 7 SKILL.md files in `UnrealEngine/skills/` | None | PLANNED |
| 2 | Developer Utility Scripts Refactoring | Update 4 `.py` scripts in `UnrealEngine/src/scripts/` | None | PLANNED |
| 3 | Automated E2E Integration Test Suite Migration | Update `Tests/test_e2e_integration.py` & run test harness | M1, M2 | PLANNED |

## Detailed Breakdown

### Milestone 1: Skill Documentation Migration
- Target Files:
  1. `UnrealEngine/skills/blueprint-authoring/SKILL.md`
  2. `UnrealEngine/skills/unreal-testing-sops/SKILL.md`
  3. `UnrealEngine/skills/add-component/SKILL.md`
  4. `UnrealEngine/skills/generate-assets/SKILL.md`
  5. `UnrealEngine/skills/setup-input/SKILL.md`
  6. `UnrealEngine/skills/setup-replication/SKILL.md`
  7. `UnrealEngine/skills/niagara-authoring/SKILL.md`
- Objectives:
  - Replace all `execute_python_script` / `unreal.*` fallback references with corresponding native C++ MCP tool routes.
  - Document parameter schemas and best practices for the 18 new tools across the skills.

### Milestone 2: Developer Utility Scripts Refactoring
- Target Files:
  1. `UnrealEngine/src/scripts/bulk_replace_references.py`
  2. `UnrealEngine/src/scripts/clean_naming_conventions.py`
  3. `UnrealEngine/src/scripts/find_unreferenced_assets.py`
  4. `UnrealEngine/src/scripts/organize_assets_by_type.py`
- Objectives:
  - Add native MCP tool route documentation & fallback wrapper methods leveraging native C++ MCP endpoints: `consolidate_asset_references`, `enforce_naming_conventions`, `find_unreferenced_assets`, `organize_assets_by_type`.

### Milestone 3: E2E Integration Test Suite Migration & Verification
- Target Files:
  1. `Tests/test_e2e_integration.py`
- Objectives:
  - Add test methods for the 18 new native C++ actions.
  - Execute `powershell -File .\Tests\run_tests.ps1` via Worker and verify 100% pass rate.
  - Independent review, challenge, and forensic audit verification.
