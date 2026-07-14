# UE-AgentFramework

UE-AgentFramework is a high-performance integration plugin that connects AI coding assistants (Claude Code, Antigravity 2.0, etc.) directly to the Unreal Engine Editor. It operates using a **Dual-MCP Server** architecture, allowing agents to read Blueprint schemas, inject node networks, parse C++ AST symbols, query world states, and perform verification tests.

The integration supports three main AI assistant platforms out-of-the-box:
1. **Antigravity 2.0** (Google's agentic AI coding environment)
2. **Claude Code** (Anthropic's terminal-based coding agent)
3. **Kilo Code** (Could easily support other platforms like OpenCode as well)

---

## Repository Structure

*   [**`AgentFramework/`**](file:///c:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/AgentFramework): Unreal Engine C++ plugin that exposes an improved Unreal Engine MCP server on port `18777` for UE editor interaction.
*   [**`UnrealEngine/`**](file:///c:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/UnrealEngine): Agent plugin containing the C++ bridge executable for the UE MCP connection, a custom AST MCP server, optimized UE skills, and agent profiles.
*   [**`Tests/`**](file:///c:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/Tests): Automated integration and unit test suite (`pytest` based) for verifying the MCP server communications.

---

## Installation & Setup

We provide an automated PowerShell installer that configures the plugin, creates dedicated python virtual environments, installs dependencies, links rules, sets up the MCP servers, indexes the whole project, generates the AST of your project's C++ code, and generates configuration files for your selected assistant.

1.  Close your Unreal Editor.
2.  Open PowerShell in the repository root and run the installer:
    ```powershell
    powershell -ExecutionPolicy Bypass -File .\UnrealEngine\install.ps1
    ```
3.  Follow the prompts to specify your target project root and choose your assistant:
    *   **Antigravity 2.0**: Generates `mcp_config.json` inside the plugin folder.
    *   **Kilo Code**: Generates `kilo.jsonc` and links rules to `.kilocode/rules/`.
    *   **Claude Code**: Generates `.mcp.json` at the project root, links skills to `.claude/skills/`, and copies rules to `CLAUDE.md`.
4.  Open your project in the Unreal Editor (compile/enable the `AgentFramework` plugin if prompted).
5.  Start your assistant session in the target project workspace and prompt:
    > `"Run project setup and index the project"`
    *(This scans your codebase, configures your local environment, and builds a token-efficient project indexing map).*


For developer environment setups, test suite operations, and detailed API documentation, refer to the files in the [**`Documentation/`**](file:///c:/Users/janv1/Documents/Unreal%20Projects/UE-Antigravity/Documentation) directory.
