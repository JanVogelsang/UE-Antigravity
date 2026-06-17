# Antigravity E2E Integration Tests

This directory contains the end-to-end integration tests for the Antigravity Dual-MCP architecture.

## Overview

The test framework connects:
1. **Internal C++ MCP Server**: Running inside Unreal Engine Editor (spawning `UnrealEditor-Cmd.exe` or reusing a running instance). It communicates via HTTP POST requests on port `18777`.
2. **External Indexing & RAG MCP Server (Python)**: Running as a standalone process communicating via JSON-RPC 2.0 over standard input/output.

## Directory Structure

- `requirements.txt`: Python package requirements for running the tests.
- `conftest.py`: pytest fixtures that manage process lifecycles (Unreal Editor and Python MCP server).
- `mock_client.py`: The `MockAgentClient` that wraps the JSON-RPC interface for the Python server and the HTTP client for the C++ server.
- `test_e2e_integration.py`: Integration tests verifying the connection and tool invocation flow for both servers.

## Getting Started

### 1. Installation
Install the necessary test dependencies using pip:
```bash
pip install -r requirements.txt
```

### 2. Configuration
The paths used to launch Unreal Engine are defined at the top of `conftest.py`:
- `UNREAL_PATH`: Absolute path to `UnrealEditor-Cmd.exe` (default: `D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe`).
- `PROJECT_PATH`: Absolute path to the `.uproject` file (default: `c:\Users\Jan\Documents\Unreal Projects\Tau.uproject`).

### 3. Running the Tests
To run the integration tests:
```bash
pytest -v
```

If the Unreal Engine editor is already open and running on your machine (listening on port 18777), the fixtures will automatically detect it and run the tests against your open instance rather than launching a new one.

If the Unreal Editor is not running and the path `UNREAL_PATH` is invalid, the tests will automatically fall back to spawning a mock C++ HTTP server in a background thread to allow local testing and continuous integration checks to succeed.
