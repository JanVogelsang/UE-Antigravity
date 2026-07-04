# AgentFramework 2.0 Development Guide & Onboarding

Welcome to the UE-AgentFramework Dual-MCP developer guide. This document contains exact instructions for compiling, testing, and debugging the codebase to ensure consistency across different agent invocations.

---

## 1. Environment Rigging (CRITICAL)

### Python Interpreter Path
On Windows systems, the default `python` or `pytest` commands on the PATH can sometimes resolve to a MinGW/MSYS2 python (`C:\msys64\ucrt64\bin\python.exe`) which lacks `pip` and `pytest`.

* **For the default development machine (user 'Jan')**:
  * **Python Path**: `C:\Users\Jan\AppData\Local\Microsoft\WindowsApps\python.exe`
  * **Pip Command**: `C:\Users\Jan\AppData\Local\Microsoft\WindowsApps\python.exe -m pip`
  * **Dependency Installation**:
    ```powershell
    & "C:\Users\Jan\AppData\Local\Microsoft\WindowsApps\python.exe" -m pip install -r Tests/requirements.txt --user
    ```
* **For other developer workspaces or CI environments**:
  * Locate the standard user python interpreter or virtual environment.
  * Install dependencies: `python -m pip install -r Tests/requirements.txt --user`.
  * Run tests using the PowerShell runner wrapper: `powershell -File .\run_tests.ps1`.

---

## 2. Compilation & C++ Builds

Unreal Engine compilation relies on UBT. Running builds concurrently can trigger a conflicting UAT instance error.

### How to Build the Plugin
Run the build script from the repository root using the UAT mutex bypass environment variable:
```powershell
$env:uebp_UATMutexNoWait = '1'
powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip
```
This command compiles the C++ plugin, packages it, and copies the latest binaries back to the game project (`tau-game/Plugins/AgentFramework`).

---

## 3. Running Automated Tests

A test runner exists at `run_tests.py` which sets up DLL directories (like `pywin32_system32` for Windows Store Python) and initializes pytest.

### How to Run Tests
Always use the PowerShell wrapper which uses the explicit Windows Store Python path:
```powershell
powershell -File .\run_tests.ps1
```

---

## 4. Key Pitfalls & Best Practices

1. **Path Casing & Slashes (Windows)**:
   Always normalize paths before executing comparison checks. MSVC/Clang outputs can mix uppercase/lowercase drive letters and backward/forward slashes:
   ```python
   # DO THIS:
   normalized_path = os.path.normcase(os.path.abspath(path))
   ```
2. **MCP Handshake Blockers**:
   Do not perform heavy synchronous logic (like parsing C++ files or updating vector databases) directly in the MCP startup lifecycle. Move heavy initialization to a background worker thread so the stdio initialization handshake completes immediately.
3. **SQLite Write-Ahead Logging (WAL)**:
   When writing to the AST cache concurrently from different threads (like watchdog event observers), use sqlite WAL mode to prevent locking:
   ```python
   conn.execute("PRAGMA journal_mode=WAL;")
   ```
