# Project: UE-AgentFramework Phase 3 Migration

## Architecture
Phase 3 focuses on replacing legacy `execute_python_script` calls with the 18 new native C++ Editor MCP actions across Skills, Scripts, and Test Suites in the UE-AgentFramework plugin repository.

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| R1 | Skill Documents Migration | Update 7 skills in `UnrealEngine/skills/` (blueprint-authoring, unreal-testing-sops, add-component, generate-assets, setup-input, setup-replication, niagara-authoring) to remove `execute_python_script` references and add native C++ MCP tool routes | None | DONE |
| R2 | Developer Utility Scripts Refactoring | Refactor 4 scripts in `UnrealEngine/src/scripts/` (bulk_replace_references.py, clean_naming_conventions.py, find_unreferenced_assets.py, organize_assets_by_type.py) to leverage native C++ MCP tool routes | R1 | DONE |
| R3 | Test Suite Update & Verification | Update `Tests/test_e2e_integration.py` to test native C++ tool routes and verify 100% pass rate via `powershell -File .\Tests\run_tests.ps1` | R1, R2 | DONE |

## Interface Contracts
- Native C++ MCP Tools: Must match signatures defined in C++ plugin implementation and summarized in `Documentation/PYTHON_FALLBACK_AUDIT.md` and `Documentation/PLUGIN_IMPROVEMENT_ROADMAP.md`.
- Test suite: `powershell -File .\Tests\run_tests.ps1` passed 100% (95 passed, 13 skipped, 0 failed).

## Code Layout
- Skills: `UnrealEngine/skills/*/SKILL.md`
- Utility Scripts: `UnrealEngine/src/scripts/*.py`
- E2E Tests: `Tests/test_e2e_integration.py` and `Tests/run_tests.ps1`
