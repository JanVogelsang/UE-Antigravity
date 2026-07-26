import os
import re
import glob

base_dir = r"c:\Users\janv1\Documents\Unreal Projects\UE-Antigravity\AgentFramework\Source\AgentFrameworkActions"
cpp_files = glob.glob(os.path.join(base_dir, "**", "*.cpp"), recursive=True)

executors = {}

for f in cpp_files:
    content = open(f, "r", encoding="utf-8", errors="ignore").read()
    if "GetSupportedToolNames" in content:
        idx = content.find("GetSupportedToolNames")
        brace_start = content.find("{", idx)
        brace_end = content.find("};", brace_start)
        block = content[brace_start:brace_end]
        tools = re.findall(r'TEXT\("([a-zA-Z0-9_]+)"\)', block)
        filename = os.path.basename(f)
        executors[filename] = tools

total_tools = sum(len(t) for t in executors.values())
print(f"Total Executors: {len(executors)}")
print(f"Total Tools: {total_tools}")
print("-" * 50)
for k, v in sorted(executors.items()):
    print(f"{k}: {len(v)} tools -> {v}")
