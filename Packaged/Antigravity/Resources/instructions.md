# Unreal Engine MCP - Global Instructions

You are connected to an Unreal Engine instance via the Antigravity MCP plugin. Whenever you perform actions in this ecosystem, you MUST obey these core rules. Think of these as the "laws of physics" for this workspace.

## 1. Asset Pathing
- **Always use Unreal Engine paths:** Paths must start with `/Game/...` (e.g., `/Game/Blueprints/BP_MyCharacter`).
- **Never use Windows paths for assets:** Do NOT use `C:/...` or relative file paths to reference `.uasset` files when using UE tools.
- **Reference format:** When an exact reference is needed by the engine, use the formal path format if required (e.g., `Blueprint'/Game/Path/To/Asset.Asset'`).

## 2. Implementation Strategy (C++ vs Blueprint)
- **Identify Project Type First:** Verify whether the target project is a C++ project (contains a `Source/` directory and `.Build.cs` / `.Target.cs` files) or a Blueprint-only project. If it is Blueprint-only, you MUST NOT write C++ code or create source files, as it will break the project unless the user has compiler tools installed. Implement all logic through Blueprint tools or Python.
- **Prefer C++ for Logic (in C++ Projects):** As an AI, you are much better at reading, writing, and reasoning about text-based C++ than generating verbose Blueprint T3D node graphs. Default to C++ for core logic, systems, and data structures.
- **Use Blueprints for Wiring/UI:** Use Blueprints primarily for assembling components, UI layouts, or simple event wiring.
- **C++ Compilation Workflow:**
  - **If the Editor is Closed:** You are free to make extensive changes to headers (`.h`), adding `UPROPERTY` and `UFUNCTION` macros as needed. Regular compilation can be used without worrying about Live Coding restrictions.
  - **If the Editor is Running:**
    - **Implementation (.cpp):** You can modify `.cpp` files freely and then trigger a compile yourself using the `trigger_compile` tool for immediate testing. **Do NOT run manual compile commands in the terminal (like running UnrealBuildTool.exe directly) if the editor is open; you must use the `trigger_compile` tool.**
    - **Headers (.h):** Modifying header files with new UHT macros (`UPROPERTY`/`UFUNCTION`) cannot be safely Live Coded. Batch your header changes together, and inform the user that they must close the editor and perform a full rebuild before the changes can be safely tested. Do not avoid UPROPERTY/UFUNCTION changes; just plan them efficiently to minimize restarts!

## 3. Safety and Editor State
- **Verify PIE is Stopped:** Before creating, modifying, or deleting assets, ensure that Play-In-Editor (PIE) is stopped. Editing assets while the game is running can cause crashes or corrupt data.
- **No File System Hacks:** NEVER use terminal/shell commands (like `rm`, `mv`, `del`) to move, rename, or delete `.uasset` or `.umap` files directly on disk. Always use the provided Unreal Engine tools. Modifying assets via the file system bypasses UE's Asset Registry and will corrupt project references.
- **Never Kill or Close the Editor Automatically:** If the editor needs to be closed or restarted (e.g., for full header rebuilds, Live Coding limits, or updating plugin binaries), you MUST NOT close or kill the editor process yourself. Instead, explain the situation to the user and request that they perform the restart for you (e.g., via Rider or by manually restarting the editor).
- **Never Kill Build Processes:** You MUST NEVER run `taskkill` or other command-line utilities to force-terminate build processes (such as `UnrealBuildTool.exe`, `cl.exe`, `link.exe`, `msbuild.exe`, etc.) to resolve compilation conflicts or lock errors (e.g., `UnrealBuildTool_Mutex` conflicts). These conflicts are transient. If a build is blocked by a running instance, wait for it to complete and retry, or gracefully notify the user.
- **Prefer trigger_compile Over Terminal Builds:** When the Unreal Editor is open, always use the `trigger_compile` tool to run builds. Do NOT trigger compiler executions via `run_command` in the terminal when the editor is running.

## 4. UI and Visual Design
- **Use Your Eyes:** When building or debugging UMG widgets or visual elements, use your vision capabilities (e.g., `capture_widget`) to evaluate the design. Do not blindly guess visual layouts without verifying the result visually.

## 5. Error Recovery and Troubleshooting
- **Validate Paths:** If a tool fails with an "asset not found" error, verify the exact path using asset listing/search tools before retrying.
- **Check Editor Logs:** If a command fails or behaves unexpectedly, use the available tools to check the Unreal Engine Output Log for detailed error messages and warnings.

## 6. C++ Standards and Unreal Engine Rules
- **Strict Naming Prefixes:** You MUST follow Unreal Engine naming conventions for C++ types. UHT will fail to compile if prefixes are incorrect:
  - `A` for Actors (e.g., `AMyCharacter`)
  - `U` for UObjects/Components (e.g., `UMyComponent`, `UUserWidget`)
  - `F` for Structs (e.g., `FMyStruct`)
  - `E` for Enums (e.g., `EMyEnum`)
  - `I` for Interfaces (e.g., `IMyInterface`)
- **Generated Headers (.generated.h):** 
  - Every header containing a `UCLASS`, `USTRUCT`, `UINTERFACE`, or `UENUM` must include the generated header as the **last** include: `#include "FileName.generated.h"`.
  - The name must exactly match the header's filename.
- **Garbage Collection Safety:** 
  - Any raw pointers to `UObject` types (including `AActor`, `UActorComponent`, etc.) must be marked with the `UPROPERTY()` macro if they are stored as class members. If omitted, they are not tracked by the reflection system and will be Garbage Collected, causing random dangling pointer crashes.
- **Module Dependencies (.Build.cs):**
  - If you use classes from specific systems (e.g., `UUserWidget` from `UMG`, GAS classes from `GameplayAbilities`, input classes from `EnhancedInput`), you must add the respective module names to the `PublicDependencyModuleNames` or `PrivateDependencyModuleNames` in the project's `<ModuleName>.Build.cs` file. Failure to do so will cause compiler/linker errors.

## 7. Tool Usage and Skill Selection
- **Prioritize Domain-Specific Tools:** Always choose the most specific tool domain (e.g., `blueprint_tools`, `widget_tools`, `cpp_tools`, `niagara_tools`) over generic file writing or terminal commands. Never use raw shell commands to perform actions that have dedicated MCP tools.
- **Prefer Macro Tools:** Use high-level Macro tools (such as `macro_create_cpp_class`) over low-level ones when creating assets or files, as they automatically handle complex boilerplate (like API export macros and UHT setup) and reduce errors.
- **Proactively Use Skills:**
  - Before executing common Unreal Engine tasks (such as setting up replication, creating actors, or adding components), check the `Antigravity/Resources/Skills/` folder for any relevant `.md` skill templates.
  - If a matching skill file exists, read its contents and follow its steps as your primary execution guideline.
- **Read Schemas for Constraints:** When using a tool for the first time, check the schema in `Antigravity/Resources/ToolSchemas/` to understand default values, constraint ranges, and required fields to prevent failed tool execution.
