# Antigravity UnrealEngine MCP Plugin

This is the Antigravity agent plugin component of the **UE-Antigravity** integration. It provides the Model Context Protocol (MCP) server configuration that allows the Antigravity AI assistant to communicate with the Unreal Engine Editor.

## Architecture

This plugin includes a lightweight C++ MCP bridge. The bridge proxies standard input/output (`stdin`/`stdout`) JSON-RPC messages from Antigravity to the Unreal Engine Editor's HTTP loopback server.

## Installation

You can load this plugin into Antigravity at the workspace level or globally:

### Workspace Level (Recommended)

Copy this `UnrealEngine` folder into the `.agents/plugins/` directory at the root of your active project workspace:

```bash
mkdir -p .agents/plugins
cp -r UnrealEngine .agents/plugins/
```

### Global Level

To make the Unreal Engine tools available across all of your projects, copy this `UnrealEngine` folder into your user profile's global configuration directory:

* **Windows:** `C:\Users\<Username>\.gemini\config\plugins\UnrealEngine`
* **Mac/Linux:** `~/.gemini/config/plugins/UnrealEngine`

## Compilation

The bridge uses a lightweight C++ executable (`bridge.exe`) to proxy requests. The agent is configured to automatically compile this binary using `src/build_bridge.bat` when it first boots and detects the binary is missing. 

Alternatively, you can compile it manually:
1. Open a terminal in the plugin directory.
2. Run `src/build_bridge.bat` (requires Visual Studio C++ build tools installed).

