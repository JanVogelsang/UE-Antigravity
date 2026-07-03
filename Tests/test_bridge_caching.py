import os
import sys
import json
import subprocess
import time
import pytest
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

CACHE_FILE = PROJECT_ROOT / "UnrealEngine" / "profiles" / "discovered_tools_cache.json"
BACKUP_FILE = PROJECT_ROOT / "UnrealEngine" / "profiles" / "discovered_tools_cache.json.bak"

@pytest.fixture(autouse=True)
def setup_cache_backup():
    # Back up existing cache file if it exists
    backup_made = False
    if CACHE_FILE.exists():
        CACHE_FILE.rename(BACKUP_FILE)
        backup_made = True
    else:
        if BACKUP_FILE.exists():
            BACKUP_FILE.unlink()
            
    yield
    
    # Restore backup
    if CACHE_FILE.exists():
        CACHE_FILE.unlink()
    if backup_made and BACKUP_FILE.exists():
        BACKUP_FILE.rename(CACHE_FILE)

def test_bridge_caching_and_fallback():
    # 1. Start the bridge process
    cmd = [sys.executable, "-u", str(PROJECT_ROOT / "UnrealEngine" / "bridge" / "main.py")]
    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )
    
    try:
        # 2. Perform initialization handshake
        init_request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "TestClient"}
            }
        }
        proc.stdin.write(json.dumps(init_request) + "\n")
        proc.stdin.flush()
        
        # Read init response
        line = proc.stdout.readline()
        assert line, "No initialization response from bridge"
        init_resp = json.loads(line)
        assert init_resp.get("id") == 1
        capabilities = init_resp.get("result", {}).get("capabilities", {})
        assert capabilities.get("tools", {}).get("listChanged") is True, "Capabilities missing listChanged: True"
        
        # Send initialized notification
        initialized = {
            "jsonrpc": "2.0",
            "method": "notifications/initialized"
        }
        proc.stdin.write(json.dumps(initialized) + "\n")
        proc.stdin.flush()
        
        # 3. Query tools (should be empty initially because editor is offline and cache is empty)
        list_req = {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "tools/list"
        }
        proc.stdin.write(json.dumps(list_req) + "\n")
        proc.stdin.flush()
        
        line = proc.stdout.readline()
        assert line, "No response to tools/list"
        list_resp = json.loads(line)
        assert list_resp.get("id") == 2
        tools = list_resp.get("result", {}).get("tools", [])
        assert len(tools) == 0, f"Expected 0 tools, got: {tools}"
        
        # 4. Write mock tool cache to the discovered_tools_cache.json
        mock_cache = {
            "tools": [
                {
                    "name": "mock_test_tool",
                    "description": "Mocked bridge tool",
                    "inputSchema": {"type": "object", "properties": {}}
                }
            ],
            "tool_owners": {
                "mock_test_tool": "native"
            }
        }
        with open(CACHE_FILE, "w") as f:
            json.dump(mock_cache, f)
            
        # 5. Wait for the background poller to detect the change and send notifications/tools/list_changed
        got_list_changed = False
        start_time = time.time()
        while time.time() - start_time < 6.0:  # Poller runs every 3.0s, so 6.0s is plenty
            line = proc.stdout.readline()
            if not line:
                break
            msg = json.loads(line)
            if msg.get("method") == "notifications/tools/list_changed":
                got_list_changed = True
                break
                
        assert got_list_changed is True, "Should have received notifications/tools/list_changed after cache file was created"
        
        # 6. Now, as a response to the notification, the client queries tools/list
        list_req_2 = {
            "jsonrpc": "2.0",
            "id": 3,
            "method": "tools/list"
        }
        proc.stdin.write(json.dumps(list_req_2) + "\n")
        proc.stdin.flush()
        
        line = proc.stdout.readline()
        assert line, "No response to tools/list query after notification"
        list_resp_2 = json.loads(line)
        assert list_resp_2.get("id") == 3
        tools_2 = list_resp_2.get("result", {}).get("tools", [])
        assert len(tools_2) == 1
        assert tools_2[0]["name"] == "mock_test_tool"
        
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
