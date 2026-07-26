# Task Description — Challenger 1 Replacement (Milestone 4: Build Verification & Test Run)

Working Directory: `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/challenger_m4_1_rep`

## Objective
Empirically verify the C++ build of the plugin and run test suite.
1. Run plugin build command:
   `powershell -ExecutionPolicy Bypass -Command "$env:uebp_UATMutexNoWait = '1'; powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip"`
2. Run pytest suite:
   `powershell -File .\Tests\run_tests.ps1`
3. Document build and test results in `verification.md` and `handoff.md` in `c:/Users/janv1/Documents/Unreal Projects/UE-Antigravity/.agents/challenger_m4_1_rep/`.
