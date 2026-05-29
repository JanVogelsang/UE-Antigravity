---
name: unreal-workflow
description: Best practices for interacting with the Unreal Engine Editor via Antigravity MCP.
---
# Unreal Workflow Skill

1. **Editor State**: All Antigravity tools require the Unreal Engine Editor to be open. If a tool returns a "connection refused" or "Editor is not running" error, politely ask the user to start their Unreal Engine project.
2. **Tool Discovery**: Tools are dynamically fetched from the active UE project. Use standard tools like `execute_console_command` to perform actions.
3. **Compilation**: If the Antigravity MCP server fails to start because `bridge.exe` is missing, you can manually trigger compilation by running `Resources/Bridge/build_bridge.bat` via shell command.
