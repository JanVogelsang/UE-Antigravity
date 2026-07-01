import sys
import json
import os

def main():
    while True:
        line = sys.stdin.readline()
        if not line:
            break
        line = line.strip()
        if not line:
            continue
        try:
            data = json.loads(line)
        except Exception:
            continue
        
        req_id = data.get("id")
        method = data.get("method")
        
        if method == "initialize":
            response = {
                "jsonrpc": "2.0",
                "result": {
                    "protocolVersion": "2024-11-05",
                    "capabilities": {},
                    "serverInfo": {
                        "name": "mock-python-server",
                        "version": "1.0.0"
                    }
                },
                "id": req_id
            }
            sys.stdout.write(json.dumps(response) + "\n")
            sys.stdout.flush()
        elif method == "tools/call":
            params = data.get("params", {})
            name = params.get("name")
            arguments = params.get("arguments", {})
            
            result_text = ""
            if name == "query_cpp_ast":
                query = arguments.get("query", "")
                result_text = f"Result of query_cpp_ast for {query}:\n" + json.dumps([
                    {
                        "name": "AMorphTargetActor",
                        "fully_qualified_name": "AMorphTargetActor",
                        "kind": "class",
                        "file_path": os.path.expandvars(r"%USERPROFILE%\Documents\Unreal Projects\tau-game\Source\AMorphTargetActor.h"),
                        "line_start": 10,
                        "line_end": 50,
                        "access_specifier": "public"
                    }
                ], indent=2)
            elif name == "generate_compile_commands":
                result_text = "Result of generate_compile_commands (succeeded):\nSTDOUT:\nGenerated successfully.\nSTDERR:\n"
            elif name == "search_vector_db":
                query = arguments.get("query", "")
                result_text = f"Result of search_vector_db for '{query}':\n" + json.dumps([
                    {
                        "title": "Mock Title",
                        "content": f"Mock content matching {query} in character movement replication.",
                        "similarity_score": 0.9,
                        "source": "MockSource.md"
                    }
                ], indent=2)
            else:
                response = {
                    "jsonrpc": "2.0",
                    "error": {
                        "code": -32601,
                        "message": f"Method not found: {name}"
                    },
                    "id": req_id
                }
                sys.stdout.write(json.dumps(response) + "\n")
                sys.stdout.flush()
                continue
                
            response = {
                "jsonrpc": "2.0",
                "result": {
                    "content": [
                        {
                            "type": "text",
                            "text": result_text
                        }
                    ]
                },
                "id": req_id
            }
            sys.stdout.write(json.dumps(response) + "\n")
            sys.stdout.flush()

if __name__ == "__main__":
    main()
