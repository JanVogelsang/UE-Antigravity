# Antigravity Unreal Engine Plugin

This is the Unreal Engine plugin component of the **UE-Antigravity** integration. It exposes an in-process HTTP loopback server (listening on port `18777`) to execute commands safely on the Game Thread, allowing the Antigravity AI coding assistant to natively interact with your Unreal Editor.

## Key Features

Exposes **70+ advanced editor tools** categorized into specialized domains:

* **Blueprint Generation**: Create Blueprint actors, add variables/functions, connect pins, and inject node graphs.
* **Widget & UMG Editor**: Retrieve widget hierarchies, modify widget slots, set fonts, and bind interactive events.
* **C++ Integration**: Create classes, modify header and implementation source files, and trigger Live Coding compilation.
* **Python Editor Scripting**: Write and execute native Unreal Python scripts inside the editor process.
* **Performance & Scalability**: Read/write cvars, analyze asset sizes, run CSV profiling, and capture rendering metrics.
* **Enhanced Input**: Define input actions, manage input mapping contexts, and bind hardware mappings.
* **Gameplay AI & World Tools**: Edit Blackboard/Behavior Tree graphs, configure NavMesh bounds, and spawn actors.

---

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

## Distribution / Epic Games Fab

When preparing this plugin for upload to the **Epic Games Fab** marketplace:
1. Ensure `Binaries/`, `Intermediate/`, and `Saved/` are deleted.
2. Zip this root plugin folder (`Antigravity/`). It is clean and ready for distribution.
