import sys
import json
import asyncio
import httpx
import os
import difflib
from pathlib import Path
from contextlib import AsyncExitStack

# Add user's pip packages if needed
import site
if sys.platform == "win32":
    site_dirs = site.getsitepackages()
    if hasattr(site, "getusersitepackages"):
        site_dirs.append(site.getusersitepackages())

try:
    from mcp.client.sse import sse_client
    from mcp.client.session import ClientSession
except ImportError:
    print("mcp package not found. Please install it.", file=sys.stderr)
    sys.exit(1)

PROFILE_DIR = Path(__file__).resolve().parent.parent / "profiles"
tool_owners = {}
ue_session = None
ue_stack = None

SYNONYM_VERBS = [
    {"spawn", "place", "create", "add"},
    {"destroy", "delete", "remove", "clear"},
    {"get", "find", "query", "read", "inspect"},
    {"set", "modify", "update", "write", "change"}
]

def load_profile(client_name=""):
    profile_name = os.environ.get("BRIDGE_PROFILE")
    if profile_name:
        path = PROFILE_DIR / f"{profile_name}.json"
        if path.exists():
            with open(path) as f: return json.load(f)
            
    if client_name:
        path = PROFILE_DIR / f"{client_name.lower()}.json"
        if path.exists():
            with open(path) as f: return json.load(f)
            
    path = PROFILE_DIR / "default.json"
    if path.exists():
        with open(path) as f: return json.load(f)
        
    return {}

def are_tools_similar(tool1, tool2):
    name1 = tool1.get("name", "").lower()
    name2 = tool2.get("name", "").lower()
    desc1 = tool1.get("description", "").lower()
    desc2 = tool2.get("description", "").lower()
    
    if name1 == name2:
        return True
        
    parts1 = name1.split('_')
    parts2 = name2.split('_')
    
    verb1 = parts1[0] if parts1 else ""
    verb2 = parts2[0] if parts2 else ""
    
    # Check if verbs are synonyms
    verbs_are_synonyms = False
    if verb1 == verb2:
        verbs_are_synonyms = True
    else:
        for group in SYNONYM_VERBS:
            if verb1 in group and verb2 in group:
                verbs_are_synonyms = True
                break
                
    if not verbs_are_synonyms:
        return False
        
    # If verbs are synonyms, check if the rest of the name is highly similar
    suffix1 = "_".join(parts1[1:])
    suffix2 = "_".join(parts2[1:])
    
    if suffix1 == suffix2:
        return True
        
    if difflib.SequenceMatcher(None, suffix1, suffix2).ratio() > 0.8:
        return True
        
    # Check description similarity as fallback
    if len(desc1) > 20 and len(desc2) > 20:
        if difflib.SequenceMatcher(None, desc1, desc2).ratio() > 0.7:
            return True
            
    return False

async def fetch_antigravity_tools():
    async with httpx.AsyncClient() as client:
        try:
            resp = await client.get("http://127.0.0.1:18777/api/tools", timeout=2.0)
            if resp.status_code == 200:
                data = resp.json()
                return data.get("tools", [])
        except Exception:
            return []
    return []

async def init_ue58_client(port):
    stack = AsyncExitStack()
    try:
        streams = await stack.enter_async_context(sse_client(f"http://127.0.0.1:{port}/sse", timeout=30.0))
        session = await stack.enter_async_context(ClientSession(streams[0], streams[1]))
        await session.initialize()
        return stack, session
    except Exception as e:
        await stack.aclose()
        return None, None

async def discover_tools(ue_port, profile):
    global tool_owners, ue_session, ue_stack
    
    native_tools = []
    if ue_session is None:
        ue_stack, ue_session = await init_ue58_client(ue_port)
        
    if ue_session:
        try:
            native_resp = await ue_session.list_tools()
            for t in native_resp.tools:
                native_tools.append({
                    "name": t.name,
                    "description": t.description or "",
                    "inputSchema": t.inputSchema or {}
                })
        except Exception as e:
            print(f"Error fetching native tools (connection lost?): {e}", file=sys.stderr)
            ue_session = None
            if ue_stack:
                await ue_stack.aclose()
                ue_stack = None
                
    ag_tools = await fetch_antigravity_tools()
    
    filtered_ag_tools = []
    for ag_tool in ag_tools:
        is_duplicate = False
        for nat_tool in native_tools:
            if are_tools_similar(ag_tool, nat_tool):
                is_duplicate = True
                break
        if not is_duplicate:
            filtered_ag_tools.append(ag_tool)
            
    final_tools = []
    tool_owners.clear()
    
    disabled_tools = profile.get("disabled_tools", [])
    tool_overrides = profile.get("tool_overrides", {})
    
    for t in native_tools:
        if t["name"] in disabled_tools: continue
        tool_owners[t["name"]] = "native"
        if t["name"] in tool_overrides:
            t.update(tool_overrides[t["name"]])
        final_tools.append(t)
        
    for t in filtered_ag_tools:
        if t["name"] in disabled_tools: continue
        tool_owners[t["name"]] = "antigravity"
        if t["name"] in tool_overrides:
            t.update(tool_overrides[t["name"]])
        final_tools.append(t)
        
    return final_tools

async def call_antigravity_tool(name, arguments, profile):
    payload = {"name": name, "arguments": arguments}
    async with httpx.AsyncClient(timeout=None) as client:
        try:
            resp = await client.post("http://127.0.0.1:18777/api/execute_tool", json=payload, timeout=None)
            ue_result = resp.json()
            bSuccess = ue_result.get("bSuccess", False)
            msg = ue_result.get("ResultMessage", "")
            
            content = []
            image_support = profile.get("image_support", False)
            image_tools = ["capture_widget", "capture_viewport", "capture_niagara_system_isolated"]
            
            if bSuccess and name in image_tools and image_support:
                base64_data = msg
                if "base64," in base64_data:
                    base64_data = base64_data.split("base64,")[1]
                content.append({
                    "type": "image",
                    "data": base64_data,
                    "mimeType": "image/png"
                })
            else:
                content.append({
                    "type": "text",
                    "text": msg
                })
                
            return {
                "content": content,
                "isError": not bSuccess
            }
        except Exception as e:
            return {
                "content": [{"type": "text", "text": str(e)}],
                "isError": True
            }

async def main_loop():
    global ue_stack, ue_session, tool_owners
    loop = asyncio.get_event_loop()
    
    # Read initialize
    line = await loop.run_in_executor(None, sys.stdin.readline)
    if not line: return
    req = json.loads(line)
    
    client_name = req.get("params", {}).get("clientInfo", {}).get("name", "")
    profile = load_profile(client_name)
    ue_port = profile.get("ue_native_mcp_port", 8000)
    
    ue_stack, ue_session = await init_ue58_client(ue_port)
    
    resp = {
        "jsonrpc": "2.0",
        "id": req["id"],
        "result": {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {"listChanged": True}},
            "serverInfo": {"name": "UnrealEngineFederated", "version": "2.1.0"}
        }
    }
    print(json.dumps(resp), flush=True)

    while True:
        line = await loop.run_in_executor(None, sys.stdin.readline)
        if not line: break
        
        try:
            msg = json.loads(line)
        except:
            continue
            
        method = msg.get("method")
        msg_id = msg.get("id")
        
        if method == "tools/list":
            final_tools = await discover_tools(ue_port, profile)
            out = {
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {"tools": final_tools}
            }
            print(json.dumps(out), flush=True)
            
        elif method == "tools/call":
            params = msg.get("params", {})
            name = params.get("name", "")
            args = params.get("arguments", {})
            
            # Lazy load tool owners if not populated or if requested tool is missing
            if not tool_owners or name not in tool_owners:
                await discover_tools(ue_port, profile)
                
            owner = tool_owners.get(name)
            result_obj = None
            
            if owner == "native":
                if ue_session is None:
                    ue_stack, ue_session = await init_ue58_client(ue_port)
                    
                if ue_session:
                    try:
                        res = await ue_session.call_tool(name, arguments=args)
                        content = []
                        for c in res.content:
                            if c.type == "text":
                                content.append({"type": "text", "text": c.text})
                            elif c.type == "image":
                                content.append({"type": "image", "data": c.data, "mimeType": c.mimeType})
                        result_obj = {
                            "content": content,
                            "isError": res.isError
                        }
                    except Exception as e:
                        ue_session = None
                        if ue_stack:
                            await ue_stack.aclose()
                            ue_stack = None
                        result_obj = {
                            "content": [{"type": "text", "text": f"Error calling native tool: {e}"}],
                            "isError": True
                        }
                else:
                    result_obj = {
                        "content": [{"type": "text", "text": "Native UE 5.8 server is not connected."}],
                        "isError": True
                    }
            elif owner == "antigravity":
                result_obj = await call_antigravity_tool(name, args, profile)
            else:
                result_obj = {
                    "content": [{"type": "text", "text": f"Tool '{name}' not found."}],
                    "isError": True
                }
                
            out = {
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": result_obj
            }
            print(json.dumps(out), flush=True)
            
        elif method == "notifications/initialized":
            pass
            
        elif method == "ping":
            out = {"jsonrpc": "2.0", "id": msg_id, "result": {}}
            print(json.dumps(out), flush=True)
            
        else:
            if msg_id is not None:
                out = {"jsonrpc": "2.0", "id": msg_id, "error": {"code": -32601, "message": f"Method '{method}' not found"}}
                print(json.dumps(out), flush=True)

    if ue_stack:
        await ue_stack.aclose()

if __name__ == "__main__":
    try:
        asyncio.run(main_loop())
    except KeyboardInterrupt:
        pass
