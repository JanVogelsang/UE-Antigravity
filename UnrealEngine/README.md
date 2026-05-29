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

## Auto-Compilation (Sidecar)

On startup, Antigravity will discover the `bridge-builder` sidecar defined in the plugin configuration. 
1. It automatically finds your Visual Studio C++ Compiler using `vswhere.exe`.
2. It compiles the bridge source (`src/main.cpp`) into `bridge.exe`.
3. The bridge starts immediately, pinging port `18777` to register tools with the AI once Unreal Engine is running.
