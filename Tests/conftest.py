import os
import sys
import subprocess
import time
import socket
import pytest
import http.server
import threading
import json
from mock_client import MockAgentClient

UNREAL_PATH = r"D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
PROJECT_PATH = r"c:\Users\Jan\Documents\Unreal Projects\tau-game\Tau.uproject"
PORT = 18777

def is_port_open(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(0.5)
        try:
            s.connect(("127.0.0.1", port))
            return True
        except (socket.timeout, ConnectionRefusedError):
            return False

@pytest.fixture(scope="session")
def unreal_process():
    """
    Spawns a mock C++ HTTP server in a background thread on port 18777
    to handle all editor-side tool execution calls.
    """
    if is_port_open(PORT):
        print(f"\nDetecting active Unreal Editor on port {PORT}. Bypassing mock server.")
        yield None
        return

    import urllib.parse
    import uuid
    import re

    # A simple mock HTTP server for editor tools
    class MockEditorHandler(http.server.BaseHTTPRequestHandler):
        # Global variable state for blueprints to support add/get roundtrip
        blueprint_vars = {
            "/Game/Blueprints/BP_ContainerTest": [
                {"name": "MyArray", "type": "TArray<float>", "category": "Default"},
                {"name": "MySet", "type": "TSet<FString>", "category": "Default"},
                {"name": "MyMap", "type": "TMap<FString, int32>", "category": "Default"}
            ],
            "/Game/Blueprint/Player/BP_RoundPawn": [
                {"name": "Health", "type": "float", "category": "Combat"}
            ],
            "/Game/Blueprints/BP_MyCharacter": []
        }

        def log_message(self, format, *args):
            pass
            
        def do_POST(self):
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            req = json.loads(post_data.decode('utf-8'))
            
            name = req.get("name")
            args = req.get("arguments", {})
            
            response = {"bSuccess": False, "ResultMessage": "", "Errors": [], "Warnings": []}
            
            if name == "get_blueprint_schema":
                asset_path = args.get("asset_path", "")
                if not asset_path:
                    response["Errors"].append("Missing required field: asset_path")
                elif asset_path == "/Game/Meshes/SM_Chair":
                    response["Errors"].append("Asset is not a Blueprint: '/Game/Meshes/SM_Chair'")
                elif asset_path == "/Game/Blueprints/BP_MyCharacter":
                    response["bSuccess"] = True
                    response["ResultMessage"] = json.dumps({
                        "asset_name": "BP_MyCharacter",
                        "parent_class": "Character",
                        "variables": MockEditorHandler.blueprint_vars.get(asset_path, []),
                        "custom_events": []
                    })
                elif asset_path == "/Game/UI/WBP_MainMenu":
                    response["bSuccess"] = True
                    response["ResultMessage"] = json.dumps({
                        "asset_name": "WBP_MainMenu",
                        "parent_class": "UserWidget",
                        "variables": [],
                        "custom_events": []
                    })
                elif asset_path == "/Game/Animations/ABP_Mannequin":
                    response["bSuccess"] = True
                    response["ResultMessage"] = json.dumps({
                        "asset_name": "ABP_Mannequin",
                        "parent_class": "AnimInstance",
                        "variables": [],
                        "custom_events": []
                    })
                elif asset_path == "/Game/Blueprints/BP_ContainerTest":
                    response["bSuccess"] = True
                    response["ResultMessage"] = json.dumps({
                        "asset_name": "BP_ContainerTest",
                        "parent_class": "Actor",
                        "variables": MockEditorHandler.blueprint_vars[asset_path],
                        "custom_events": []
                    })
                elif asset_path == "/Game/Blueprint/Player/BP_RoundPawn":
                    response["bSuccess"] = True
                    response["ResultMessage"] = json.dumps({
                        "asset_name": "BP_RoundPawn",
                        "parent_class": "Pawn",
                        "variables": MockEditorHandler.blueprint_vars[asset_path],
                        "custom_events": []
                    })
                else:
                    response["Errors"].append(f"Failed to load Blueprint fallback: '{asset_path}'")
                    
            elif name == "inject_blueprint_nodes_t3d":
                asset_path = args.get("asset_path", "")
                t3d_text = args.get("t3d_text", "")
                if not asset_path:
                    response["Errors"].append("Blueprint not found")
                else:
                    response["bSuccess"] = True
                    # Resolve placeholders safely (sort by length descending to prevent prefix corruption)
                    placeholders = set(re.findall(r'(GUID_[A-Za-z0-9_]+|LINK_[A-Za-z0-9_]+|NODEREF_[A-Za-z0-9_]+)', t3d_text))
                    sorted_placeholders = sorted(placeholders, key=len, reverse=True)
                    resolved_t3d = t3d_text
                    for p in sorted_placeholders:
                        resolved_t3d = resolved_t3d.replace(p, uuid.uuid4().hex)
                    response["ResultMessage"] = (
                        f"Injected 3 nodes into graph 'EventGraph' of '{asset_path}'.\n"
                        f"Compile: SUCCESS.\n"
                        f"ResolvedT3D:\n{resolved_t3d}"
                    )
                    
            elif name == "get_cpp_reflection_info":
                class_name = args.get("class_name", "")
                if class_name in ("Actor", "AActor"):
                    response["bSuccess"] = True
                    res_obj = {"class_name": "Actor", "parent_class": "Object", "is_abstract": False, "is_blueprint_spawnable": True}
                    if args.get("include_properties", True):
                        res_obj["properties"] = []
                    if args.get("include_functions", True):
                        res_obj["functions"] = []
                    if args.get("include_interfaces", True):
                        res_obj["interfaces"] = []
                    if args.get("include_metadata", True):
                        res_obj["metadata"] = {}
                    response["ResultMessage"] = json.dumps(res_obj)
                else:
                    response["Errors"].append(f"Class '{class_name}' not found")
                    
            elif name == "add_blueprint_variable":
                asset_path = args.get("asset_path", "")
                var_name = args.get("variable_name", "")
                var_type = args.get("variable_type", "")
                
                # Normalize types for roundtrip comparison
                if "TArray" in var_type:
                    var_type = "TArray<float>"
                elif "TSet" in var_type:
                    var_type = "TSet<FString>"
                elif "TMap" in var_type:
                    var_type = "TMap<FString, int32>"
                    
                if asset_path in MockEditorHandler.blueprint_vars:
                    # check if already exists
                    if not any(v["name"] == var_name for v in MockEditorHandler.blueprint_vars[asset_path]):
                        MockEditorHandler.blueprint_vars[asset_path].append({
                            "name": var_name,
                            "type": var_type,
                            "category": args.get("category", "Default")
                        })
                response["bSuccess"] = True
                response["ResultMessage"] = "Added variable successfully."
                
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(response).encode('utf-8'))
            
        def do_GET(self):
            if self.path == "/api/tools":
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({"tools": []}).encode('utf-8'))
            else:
                self.send_response(404)
                self.end_headers()

    server = http.server.HTTPServer(("127.0.0.1", PORT), MockEditorHandler)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    print(f"\nStarted mock C++ HTTP server on port {PORT}")
    
    yield server
    
    server.shutdown()
    server.server_close()
    print(f"\nStopped mock C++ HTTP server on port {PORT}")

@pytest.fixture(scope="session")
def is_live_editor(unreal_process):
    return unreal_process is None

@pytest.fixture(scope="session")
def unreal_port_wait(unreal_process):
    """
    Polls port 18777 with a 120-second timeout until it is open and responds to a readiness probe.
    """
    import urllib.request
    import json
    start_time = time.time()
    timeout = 120.0
    success = False

    print(f"\nPolling port {PORT} with readiness probe (timeout: {timeout}s)...")
    while time.time() - start_time < timeout:
        if is_port_open(PORT):
            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{PORT}/api/tools", timeout=2.0) as response:
                    if response.status == 200:
                        data = json.loads(response.read().decode('utf-8'))
                        if isinstance(data, list) or (isinstance(data, dict) and isinstance(data.get("tools"), list)):
                            success = True
                            break
            except Exception as e:
                # Readiness probe request failed, retry
                pass
        time.sleep(0.5)

    if not success:
        pytest.exit(f"Readiness probe on port {PORT} failed or did not return a valid list within {timeout} seconds.")

    print(f"Port {PORT} readiness probe succeeded. Proceeding to run tests.")
    yield

@pytest.fixture(scope="session")
def python_server_process():
    """
    Spawns the Python server process as a subprocess running 'python -m ExternalServer.src.main'.
    """
    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    
    env = os.environ.copy()
    if "PYTHONPATH" in env:
        env["PYTHONPATH"] = f"{project_root};{env['PYTHONPATH']}"
    else:
        env["PYTHONPATH"] = project_root

    cmd = [sys.executable, "-u", "-m", "UnrealEngine.ExternalServer.src.main"]
    print(f"\nSpawning Python MCP server process with command: {' '.join(cmd)}")
    
    process = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=project_root,
        env=env,
        text=True,
        bufsize=1
    )

    yield process

    # Teardown
    if process.poll() is None:
        print("\nTerminating Python MCP server process...")
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()

@pytest.fixture(scope="session")
def mock_agent_client(unreal_port_wait, python_server_process):
    """
    Yields a mock agent client fixture connected to both the C++ and Python MCP servers.
    """
    client = MockAgentClient(
        python_process=python_server_process,
        cpp_url=f"http://127.0.0.1:{PORT}"
    )
    yield client
