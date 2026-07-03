# UE-Antigravity - Agent Guidance

This file contains workspace-specific rules, constraints, and instructions for AI agents operating in this project.

## Agent Efficiency & Robustness Guidelines
- **MCP Tool Priority**: Always prefer native Unreal Engine MCP tools and dedicated skills over `execute_python_script`. Only use `execute_python_script` when no native tool or dedicated skill exists for the specific task. If you must use `execute_python_script`, you are required to provide a detailed justification in the `justification_why_native_tools_or_skills_are_insufficient` parameter. Using Python to bypass specific tools/skills is strictly prohibited.
- **Wait Durations & Scheduling**: Avoid active polling loops or shell-based sleep commands. Use the platform's native `schedule` tool (with `DurationSeconds`) when waiting for long background tasks. Ensure wait commands/timeouts for Editor start, compilation, and PIE loading are realistic (e.g., 30-90 seconds for Editor start, and 60-180 seconds for C++ compilation) to prevent premature check-in timeouts.
- **Log Parsing Efficiency**: When reading active logs via commands, always use `Get-Content -Path <LogPath> -Tail <N>` (PowerShell) or `tail -n <N>` (Unix/macOS) instead of reading the entire file.
- **Precision File Reading with `view_file`**: Use the native `view_file` tool rather than command-line utilities (like `cat` or `Get-Content`) to view source files. Specify `StartLine` and `EndLine` whenever possible to read only the target lines, keeping token overhead minimal.
- **Handling Failures and Stuck Processes**: If an editor session, compilation command, or automation test fails or hangs, do not retry blindly. Check the tail of the log file, report the issue clearly, and immediately ask the user for assistance or verification.
- **Blueprint Node Generation**: When generating plain-text T3D representations of Blueprint nodes to import/paste, always include a unique, valid `NodeGuid` parameter (a 32-character uppercase hexadecimal string) for each node. If omitted, the imported nodes will lack GUIDs, causing "missing NodeGuid" warnings during cooking.
- **Compilation Database**: Whenever you add new C++ source files, include new headers, or change project dependencies, automatically invoke the `generate_compile_commands` tool to regenerate the `compile_commands.json` database. This ensures the AST indexing and C++ context remain up-to-date.
- **Windows Path Escaping**: When launching Unreal Engine or running `Build.bat` commands in PowerShell or cmd, never include a trailing backslash inside double-quoted paths (e.g., use `"C:\Path\To\Project"` or `"C:\Path\To\Project\Project.uproject"` instead of `"C:\Path\To\Project\"`). A trailing backslash inside double quotes escapes the closing quote on Windows, leading to argument corruption and the "Failed to open descriptor file" error.

## Project Knowledge & Navigation
- **Consulting the Project Index**: The entry-point skill `project-index` is auto-loaded by the assistant runner, but the detailed sub-documents in the `references/` subdirectory are not. When you need deep architectural context, gameplay specifications, or system designs, you **MUST** manually read the relevant reference documents under `.agents/skills/project-index/references/` (e.g., `concepts.md`, `gameplay.md`, `systems.md`, `files_index.md`) using the `view_file` tool to align your work with the project's pillars and conventions.
  * Always consult the documentation index and symbol directories (e.g., `files_index.md`) first to locate specific assets or screen classes before running generic directory searches (`Get-ChildItem -Recurse`).
- **AST & Header Verification**:
  * Before calling any class method, you MUST verify its exact signature in its respective C++ header file. Do NOT guess names or properties (e.g., verify `GetCurrency()` instead of guessing `GetCurrencyPoints()`).
  * Utilize the `compile_commands.json` database or lookups to trace unfamiliar types or includes.
- **Automation Test Run Safeguards**:
  * When running automated CLI tests via `UnrealEditor-Cmd.exe`, always append `; Quit` to the end of the `-ExecCmds` string (e.g. `-ExecCmds="Automation RunTests [ProjectOrTestName]; Quit"`). This prevents standalone engine processes from hanging indefinitely.
- **Ticking Guardrails**:
  * Never place high-overhead logic (like widget tree iterations, attribute matching, or regex lookups) inside ticking functions (`NativeTick`, `Tick`).
  * Ensure any array access inside a ticking function is preceded by a bounds check or `INDEX_NONE` check to prevent Access Violation crashes.

## The Unreal Engine "Ponytail" Ladder
When writing C++ or Blueprint code, you MUST follow this strict ladder of evaluation. Never write code without checking these rungs first:

1. **Does this need to exist? (YAGNI):** Avoid over-engineering complex C++ class hierarchies or massive Blueprint subsystems if a simpler, localized solution works.
2. **Already in this codebase? (Reuse):** Aggressively search for existing project-specific `UBlueprintFunctionLibrary` classes, base classes, or macros before creating new ones.
3. **Does the Engine do it? (Stdlib/Native Platform):** The most important rung for UE. Before writing custom logic, check:
   * Is there a built-in node for this? (e.g., using `FMath` or `UKismetMathLibrary` instead of custom math).
   * Is there an Engine subsystem that already handles this? (e.g., using *Enhanced Input*, *Navigation System*, or *Gameplay Ability System*).
4. **Is there a Plugin for it? (Installed Dependency):** If the project has enabled specific plugins, utilize their features rather than writing redundant native code.
5. **One line / One Node:** Can this be solved with a simple engine macro or a single specialized node?
6. **Only then: The minimum that works.**

**Lazy, not Negligent (Caveats):**
You must NEVER compromise on safety:
* **Null Checks:** Missing `IsValid()` or `nullptr` checks will hard-crash the UE Editor. This is non-negotiable.
* **Memory Management:** Proper use of `UPROPERTY()` for garbage collection and `TWeakObjectPtr` where appropriate.
* **Multiplayer Safety:** Do not take shortcuts that break Server/Client authority (e.g., skipping `Server_` RPCs or `ReplicatedUsing`).
