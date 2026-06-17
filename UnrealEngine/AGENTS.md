# UE-Antigravity - Agent Guidance

This directory contains the agent plugin component of the UE-Antigravity integration.

## Agent Efficiency & Robustness Guidelines
- **Wait Durations & Scheduling**: Avoid active polling loops or shell-based sleep commands. Use the platform's native `schedule` tool (with `DurationSeconds`) when waiting for long background tasks. Ensure wait commands/timeouts for Editor start, compilation, and PIE loading are realistic (e.g., 5-15 seconds) to prevent premature check-in timeouts.
- **Log Parsing Efficiency**: When reading active logs via commands, always use `Get-Content -Path <LogPath> -Tail <N>` (PowerShell) or `tail -n <N>` (Unix/macOS) instead of reading the entire file.
- **Precision File Reading with `view_file`**: Use the native `view_file` tool rather than command-line utilities (like `cat` or `Get-Content`) to view source files. Specify `StartLine` and `EndLine` whenever possible to read only the target lines, keeping token overhead minimal.
- **Handling Failures and Stuck Processes**: If an editor session, compilation command, or automation test fails or hangs, do not retry blindly. Check the tail of the log file, report the issue clearly, and immediately ask the user for assistance or verification.
- **Blueprint Node Generation**: When generating plain-text T3D representations of Blueprint nodes to import/paste, always include a unique, valid `NodeGuid` parameter (a 32-character uppercase hexadecimal string) for each node. If omitted, the imported nodes will lack GUIDs, causing "missing NodeGuid" warnings during cooking.
- **Compilation Database**: Whenever you add new C++ source files, include new headers, or change project dependencies, automatically invoke the `generate_compile_commands` tool to regenerate the `compile_commands.json` database. This ensures the AST indexing and C++ context remain up-to-date.
