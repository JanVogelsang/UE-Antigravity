# Antigravity UnrealEngine MCP Plugin

This is the Antigravity agent plugin component of the **UE-Antigravity** integration. It provides the Model Context Protocol (MCP) server configuration that allows AI assistants to communicate with the Unreal Engine Editor.

## Architecture

This plugin includes a lightweight C++ MCP bridge. The bridge proxies standard input/output (`stdin`/`stdout`) JSON-RPC messages from any MCP-compatible AI client to the Unreal Engine Editor's HTTP loopback server.

### LLM Profile System

The bridge supports a **profile-based configuration** that automatically adapts tool schemas, image handling, and tool visibility to the capabilities of the active LLM.

**Resolution order:**
1. `BRIDGE_PROFILE` environment variable (highest priority)
2. Auto-detection from MCP `clientInfo.name` (e.g., Antigravity is detected automatically)
3. `profiles/default.json` (fallback)

#### Available Profiles

| Profile | Image Support | Tool Overrides | Use Case |
|---|---|---|---|
| `default` | No | None | Safe fallback for unknown clients |
| `antigravity` | Yes | None | Auto-detected when using Antigravity |
| `deepseek-v4` | No | CoT triggers, strict schemas | DeepSeek V4 via Kilo Code |
| `kimi-k2` | No | Bilingual CoT (EN/ZH) | Kimi K2.6 via Kilo Code |
| `claude` | Yes | None | Claude via Kilo Code or Claude Desktop |

#### Profile Structure

Each profile JSON file supports the following fields:

```json
{
  "image_support": true,
  "disabled_tools": ["tool_name_to_hide"],
  "tool_overrides": {
    "tool_name": {
      "description": "Override description",
      "inputSchema": { }
    }
  }
}
```

- **`image_support`**: When `true`, image-returning tools (e.g., `capture_widget`) emit standard MCP `{"type": "image"}` content blocks. When `false`, they emit plain text.
- **`disabled_tools`**: Array of tool names to completely hide from the client's `tools/list` response.
- **`tool_overrides`**: Per-tool overrides for `description` and `inputSchema`. Useful for injecting Chain-of-Thought reasoning parameters or stricter validation for models that need it.

#### Creating Custom Profiles

To add support for a new LLM, create a new JSON file in the `profiles/` directory (e.g., `profiles/my-model.json`) following the structure above. Set `BRIDGE_PROFILE=my-model` in your client configuration.

## Supported Clients

### Antigravity (Auto-detected)

No additional configuration is needed. The bridge automatically detects Antigravity via the MCP `clientInfo` handshake and loads the `antigravity` profile.

### Kilo Code (JetBrains IDEs)

Configure the MCP server in your project's `kilo.jsonc`:

```jsonc
{
  "mcpServers": {
    "unrealengine": {
      "type": "stdio",
      "command": "C:\\path\\to\\bridge.exe",
      "env": {
        "BRIDGE_PROFILE": "deepseek-v4"
      },
      "enabled": true
    }
  }
}
```

Copy the skill file to `.kilocode/rules/unreal-workflow.md` so the LLM follows the standard operating procedures.

### Other MCP Clients (Claude Desktop, Cursor, etc.)

Any client that supports the MCP stdio transport can use this bridge. Configure the server command pointing to `bridge.exe` and set the `BRIDGE_PROFILE` environment variable to match your model.

## Installation

### Quick Install (Recommended)

Run the installation script from the plugin directory:

```powershell
.\install.ps1
```

The script will:
1. Ask for your target project root directory.
2. Compile the bridge if needed.
3. Copy the plugin files to `.agents/plugins/UnrealEngine/` for Antigravity.
4. Create a hardlink for the skill file in `.kilocode/rules/` for Kilo Code.
5. Let you select an LLM profile and generate `kilo.jsonc`.

### Manual Installation

#### Workspace Level

Copy the `UnrealEngine` folder into the `.agents/plugins/` directory at the root of your project:

```bash
mkdir -p .agents/plugins
cp -r UnrealEngine .agents/plugins/
```

#### Global Level

Copy to your user profile's global configuration directory:

* **Windows:** `C:\Users\<Username>\.gemini\config\plugins\UnrealEngine`
* **Mac/Linux:** `~/.gemini/config/plugins/UnrealEngine`

## Compilation

The bridge uses a lightweight C++ executable (`bridge.exe`) to proxy requests. The agent is configured to automatically compile this binary using `src/build_bridge.bat` when it first boots and detects the binary is missing.

Alternatively, you can compile it manually:
1. Open a terminal in the plugin directory.
2. Run `src/build_bridge.bat` (requires Visual Studio C++ build tools installed).
