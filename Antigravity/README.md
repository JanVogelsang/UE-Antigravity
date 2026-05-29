# Antigravity Unreal Engine Plugin

This is the Unreal Engine plugin component of the **UE-Antigravity** integration. It exposes an in-process HTTP loopback server (listening on port `18777`) to execute commands safely on the Game Thread, allowing the Antigravity AI coding assistant to natively interact with your Unreal Editor.

## Installation

1. Create a `Plugins` directory at the root of your Unreal Engine project if it doesn't already exist.
2. Copy this `Antigravity` folder inside your project's `Plugins` directory:
   ```
   YourProject/
   ├── Plugins/
   │   └── Antigravity/
   ```
3. Open your project in Unreal Editor. If prompted to rebuild missing modules, click **Yes**.
4. Make sure the plugin is enabled in **Edit > Plugins**.
