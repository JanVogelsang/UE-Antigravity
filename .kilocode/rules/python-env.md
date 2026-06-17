# Python Environment & Testing Guide

This rule defines the requirements, environment details, and execution commands for Python development and automated testing in this repository.

---

## 1. Environment Rigging & Python Path (CRITICAL)

On this system, the default `python` or `pytest` commands on the PATH resolve to an MSYS2/MinGW Python (`C:\msys64\ucrt64\bin\python.exe`), which lacks required modules (`pytest`, `requests`, `mcp`, `watchdog`).

* **Always use the explicit Windows Store Python interpreter**:
  * **Python Path**: `C:\Users\Jan\AppData\Local\Microsoft\WindowsApps\python.exe`
  * **Pip Command**: `C:\Users\Jan\AppData\Local\Microsoft\WindowsApps\python.exe -m pip`
* **Dependency Installation**:
  ```powershell
  & "C:\Users\Jan\AppData\Local\Microsoft\WindowsApps\python.exe" -m pip install -r Tests/requirements.txt --user
  ```
* **Running the Test Suite**:
  Do **not** run raw `pytest` or `python -m pytest`. Instead, run the PowerShell wrapper or execute the runner script directly:
  ```powershell
  powershell -File .\run_tests.ps1
  # OR
  & "C:\Users\Jan\AppData\Local\Microsoft\WindowsApps\python.exe" run_tests.py
  ```

---

## 2. C++ Builds & Compilation

Unreal Engine builds are managed via the Unreal Automation Tool (UAT).
* To prevent conflicting concurrent build session locks, you must set the mutex bypass environment variable before compilation:
  ```powershell
  $env:uebp_UATMutexNoWait = '1'
  powershell -ExecutionPolicy Bypass -File .\build_plugin.ps1 -NoZip
  ```

---

## 3. Path Normalization on Windows

Windows is case-insensitive, but SQLite queries, file watchers, and C++ Clang parser paths can perform case-sensitive comparisons or output mixed slashes (e.g. `c:\` vs `C:\` or `/` vs `\`).
* **Rule**: Always normalize file paths before performing lookups, indexing, or unit test assertions:
  ```python
  normalized_path = os.path.normcase(os.path.abspath(path))
  ```

---

## 4. MCP JSON-RPC & Stdio IPC Pitfalls

If you are developing or running the External Python MCP server or any stdio-based subprocesses:
1. **Unbuffered Output**: Python buffers stdout by default when piped. This will hang JSON-RPC handshake readers. Always launch python subprocesses with the `-u` (unbuffered) flag:
   ```bash
   python -u -m ExternalServer.src.main
   ```
2. **Stdout Pollution**: The stdio channel is dedicated solely to JSON-RPC messages. Any logs, third-party logs, or subprocess commands (like `pip install`) that write raw text to `stdout` will pollute the channel and crash the client parser.
   * **Rule**: Always redirect stdout of internal commands to `stderr` or `subprocess.DEVNULL`:
     ```python
     subprocess.run([sys.executable, "-m", "pip", "install", ...], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
     ```
3. **Non-Blocking Handshake**: Do not perform heavy synchronous blocking actions (like AST parsing or index building) during the MCP server startup lifecycle. Move heavy initialization to a background worker thread so the stdio initialization handshake completes immediately.
4. **Placeholder Substitution**: When replacing placeholders (e.g. `LINK_1`, `LINK_10`) in T3D node definitions, sort placeholders by length descending before replacement to prevent prefix collisions (e.g. replacing `LINK_10` with the value of `LINK_1` + `0`).
