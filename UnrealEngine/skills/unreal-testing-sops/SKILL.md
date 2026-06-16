---
name: unreal-testing-sops
description: Standard Operating Procedures (SOPs) for testing game functionality in Unreal Engine.
---
# Unreal Engine Testing SOPs

This skill contains Standard Operating Procedures (SOPs) for testing game functionality in Unreal Engine. Following these SOPs ensures reliable testing with minimal token usage.

## SOP Design Principles for Minimal Token Usage
To minimize Antigravity quota and token consumption:
1. **Avoid Viewport Captures (`capture_viewport` / `capture_widget`)**: Analyzing images with multimodal prompts is extremely expensive. Do not capture images unless verifying UI visuals.
2. **Prefer Keystroke Sequences (`simulate_input`)**: Navigate menus using keyboard/gamepad focus keys (e.g., `Tab`, `Arrow Keys`, `Enter`, `SpaceBar`, or Gamepad face buttons).
3. **Use Direct Bindings or Actions**: If a screen binds an action to a key (like `IA_StartRound` bound to `Enter` or a gamepad button), trigger that key directly rather than navigating to and clicking the button.
4. **Use Console Commands (`execute_console_command`)**: Bypass UI when testing systems (e.g. `open LevelName` or cheat commands) to minimize setup time.
5. **Use Native `view_file` with Start/End Lines**: Avoid `cat` or `Get-Content` for reading source files. Use the native `view_file` tool with specific line ranges to read only the code of interest.
6. **Log Parsing Efficiency**: When checking log files for activity, use `Get-Content -Path <LogPath> -Tail <N>` (PowerShell) or `tail -n <N>` (Unix/macOS) rather than reading entire log files.
7. **Realistic Wait Durations & Native Scheduler**: Use the native `schedule` tool (with `DurationSeconds`) for one-shot timer delays rather than command-line sleep/polling loops. Use realistic timeouts (e.g., 5-15s) for map loading, PIE startup, and compilation.
8. **Fail Fast & Escalate**: Do not loop/retry blindly on failed commands or unresponsive editor sessions. Report the issue clearly with log details and ask the user for help.

---

## SOP-001: PvP Matchmaking & Round Start

### Goal
Verify the PvP matchmaking flow from the Main Menu to the Fleet Management screen, and initiate the combat round.

### Prerequisites
- Unreal Editor is open.
- The project builds and matches the current commit.

### Execution Workflows

#### Option A: Direct UI Action Triggering (Most Cost-Efficient / Recommended)
This method utilizes bound input actions to trigger UI handlers directly, bypassing the need to visually locate or click elements.

1. **Start PIE Session**:
   - Tool: `start_pie_session`
2. **Navigate Main Menu**:
   - Simulating gamepad/keyboard navigation to focus and select the PvP button:
     - Tool: `simulate_input` -> `key`: `"Tab"` (to move focus to the PvPButton)
     - Tool: `simulate_input` -> `key`: `"Enter"` (to click the focused button)
   - *Alternative (if focus is already default)*:
     - Tool: `simulate_input` -> `key`: `"Enter"` or `"Gamepad_FaceButton_Bottom"`
3. **Wait for Loading**:
   - Pause execution for a brief moment (e.g. 2-3 seconds) to allow the fleet management screen (`W_FleetManagementUI`) to load and register.
4. **Trigger Start Round Action**:
   - The `UFleetManagementUI` screen binds the `IA_StartRound` action to `HandleStartRound`.
   - Instead of mouse-clicking the button, simulate the key mapped to `IA_StartRound` (e.g., `Enter` or the specified key mapping):
     - Tool: `simulate_input` -> `key`: `"Enter"` (or the mapped key name)

#### Option B: Debug Console Commands (Highly Recommended for Agent Automation)
This method utilizes the custom C++ executive debug commands to jump directly to the target screens and trigger actions, requiring minimal token overhead and no image processing.

1. **Start PIE Session**:
   - Tool: `start_pie_session`
2. **Trigger PvP Matchmaking Transition (Main Menu)**:
   - Tool: `execute_console_command` -> `command`: `"TauDebugGoToPvP"`
3. **Wait for Loading**:
   - Pause execution for 2-3 seconds.
4. **Trigger Start Round (Fleet Management)**:
   - Tool: `execute_console_command` -> `command`: `"TauDebugStartRound"`

#### Option C: Visual Mouse-Clicking (Fallback - High Token Cost)
Use this option only if input action bindings and debug console commands are both unavailable.

1. **Start PIE Session**:
   - Tool: `start_pie_session`
2. **Locate PvP Button Visually**:
   - Tool: `capture_viewport` (costs image tokens)
   - *Reasoning*: Locate the "PvPButton" in the captured image.
3. **Click PvP Button**:
   - Use a Python script via `execute_python_script` to programmatically trigger the button click event (which avoids coordinates altogether and is more robust than coordinate clicks):
     ```python
     import unreal
     # Get the Main Menu widget and call click on the PvPButton
     widgets = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(unreal.EditorLevelLibrary.get_editor_world(), unreal.UserWidget, True)
     for w in widgets:
         if w.get_name() == "W_TauMainMenu_C" or "MainMenu" in w.get_name():
             # Find PvPButton and call Click/OnClicked
             pvp_button = w.get_editor_property("PvPButton")
             if pvp_button:
                 pvp_button.on_clicked.broadcast()
                 print("PvP button clicked programmatically")
                 break
     ```
4. **Wait for Loading**:
   - Pause execution for 2-3 seconds.
5. **Locate and Click Start Round Button**:
   - Programmatically click the `StartRoundButton` on `W_FleetManagementUI`:
     ```python
     import unreal
     widgets = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(unreal.EditorLevelLibrary.get_editor_world(), unreal.UserWidget, True)
     for w in widgets:
         if "FleetManagement" in w.get_name():
             start_button = w.get_editor_property("StartRoundButton")
             if start_button:
                 start_button.on_clicked.broadcast()
                 print("Start Round button clicked programmatically")
                 break
     ```

### Verification & Success Criteria
- The game state transitions to `TauRoundGameMode` (Combat Phase).
- The logs show `HandleStartRound` execution and the spawning of squads on the battlefield.
- Verify using `read_message_log` or viewport frame inspection that the match starts successfully.

---

## SOP-002: Automated Performance Testing & Analysis

### Goal
Verify the performance of the game under high-entity counts by running a simulated spectator round, programmatically capturing Unreal Insights trace and CSV profile data, and automatically generating an optimization diagnostic report in JSON.

### Prerequisites
- Unreal Editor is open.
- The project builds and matches the current commit.
- In-game graphics quality settings are at their target profile.

### Execution Workflow (Console Commands / Automation)
This is the workflow for the Antigravity agent to test performance, analyze results, and report bottlenecks:

1. **(Optional) Configure Custom Benchmark Settings**:
   - By default, the benchmark spawns 800 entities and runs for 10 seconds. To configure custom settings, set the following console variables (CVars):
     - Tool: `execute_console_command` -> `command`: `"Tau.Test.BenchmarkEntities <count>"`
     - Tool: `execute_console_command` -> `command`: `"Tau.Test.BenchmarkDuration <seconds>"`
2. **Ensure PIE safety**:
   - If a Play-In-Editor (PIE) session is currently active, ensure it is closed to avoid conflicts:
     - Tool: `stop_pie_session` (if running)
3. **Run the Automation Test**:
   - Tool: `run_automation_tests` -> `test_filter`: `"Tau.Performance.Benchmark"`
4. **Verify Test Success**:
   - Ensure the test returns success. (Background throttling is automatically disabled by the Antigravity core plugin on startup and during the trace).
5. **Agent Analysis & Optimization Recommendations**:
   - Run the custom Python analysis script to filter the data, calculate frame time spikes, analyze parallel CPU execution/concurrency, and produce a query index:
     - Tool: `run_command` -> `CommandLine`: `"python .agents/scripts/parse_benchmark.py"`
   - Present the returned JSON report summary (including the identified bottleneck, frame spikes, longest-running Mass and Niagara tasks, concurrency overlap details, and the targeted Query Index) to the user.

### Verification & Success Criteria
- The benchmark runs for the specified duration and exits cleanly.
- `Saved/Performance/BenchmarkResult.json` is generated and contains the following JSON structure:
  ```json
  {
    "SimulationTimeSeconds": 15.0,
    "StartActiveUnits": 1000,
    "EndActiveUnits": 980,
    "PrimaryBottleneck": "Game Thread",
    "ClientFPS": { "Average": 45.2, "Min": 28.5, "Max": 60.1 },
    "GameThread": { "AverageMs": 16.5, "MinMs": 10.2, "MaxMs": 35.1 },
    "RenderThread": { "AverageMs": 12.1, "MinMs": 8.5, "MaxMs": 20.2 },
    "GPU": { "AverageMs": 14.2, "MinMs": 10.5, "MaxMs": 18.1 },
    "Recommendations": [
      "Game Thread is the primary bottleneck. Review CPU performance.",
      "Mass Entity System update time is high. Consider optimizing UUnitBehaviorProcessor logic or fragment sizes."
    ]
  }
  ```
