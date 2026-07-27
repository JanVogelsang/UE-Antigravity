"""
Export and Audit Action Schemas Utility Script.
Scans all C++ action executor registration files in AgentFramework/Source/AgentFrameworkActions/Private
and verifies that every registered C++ tool action is represented in AgentFramework/Resources/ToolSchemas/*.json.
"""

import json
import os
import re
import sys

def audit_schemas(plugin_dir):
    actions_src_dir = os.path.join(plugin_dir, "AgentFramework", "Source", "AgentFrameworkActions", "Private")
    schemas_dir = os.path.join(plugin_dir, "AgentFramework", "Resources", "ToolSchemas")

    if not os.path.exists(actions_src_dir) or not os.path.exists(schemas_dir):
        print(f"Error: Directory not found: {actions_src_dir} or {schemas_dir}")
        return False

    # 1. Parse all registered C++ action names
    cpp_tool_names = set()
    # Match patterns like: Action == TEXT("tool_name"), RegisterAction(TEXT("tool_name")), ExecuteAction(TEXT("tool_name"))
    tool_patterns = [
        re.compile(r'Action\s*==\s*TEXT\("([a-z0-9_]+)"\)'),
        re.compile(r'RegisterAction\s*\(\s*TEXT\("([a-z0-9_]+)"\)'),
        re.compile(r'RegisterTool\s*\(\s*TEXT\("([a-z0-9_]+)"\)')
    ]
    
    for root, _, files in os.walk(actions_src_dir):
        for file in files:
            if file.endswith(".cpp"):
                file_path = os.path.join(root, file)
                with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                    content = f.read()
                    for pattern in tool_patterns:
                        for match in pattern.findall(content):
                            if "_" in match or match in ["compile", "build", "save", "load", "ping"]:
                                cpp_tool_names.add(match)

    # 2. Parse all JSON schemas
    json_tool_names = set()
    schema_files = [f for f in os.listdir(schemas_dir) if f.endswith(".json")]
    
    for sf in schema_files:
        sf_path = os.path.join(schemas_dir, sf)
        try:
            with open(sf_path, "r", encoding="utf-8") as f:
                data = json.load(f)
                tools = data.get("tools", [])
                for t in tools:
                    if "name" in t:
                        json_tool_names.add(t["name"])
        except Exception as e:
            print(f"Error parsing JSON schema {sf}: {e}")

    # 3. Compute audit gaps
    missing_in_json = sorted(list(cpp_tool_names - json_tool_names))
    
    print(f"Total C++ Tool Registrations Identified: {len(cpp_tool_names)}")
    print(f"Total JSON Schema Definitions Found: {len(json_tool_names)}")
    print(f"Schema Coverage Rate: {(len(json_tool_names - (json_tool_names - cpp_tool_names)) / len(cpp_tool_names) * 100):.1f}%")
    
    if missing_in_json:
        print(f"\n[WARNING] {len(missing_in_json)} C++ tools missing from static JSON schema files:")
        for m in missing_in_json:
            print(f"  - {m}")
    else:
        print("\n[SUCCESS] 100% of C++ tools are covered by static JSON schema files!")

    return len(missing_in_json) == 0

if __name__ == "__main__":
    plugin_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
    success = audit_schemas(plugin_root)
    sys.exit(0 if success else 1)
