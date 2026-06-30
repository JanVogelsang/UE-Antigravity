import re
import logging

logger = logging.getLogger("t3d_layout")

def parse_pin_line(line: str) -> dict:
    pin_id_match = re.search(r'PinId=([^,\s\)]+)', line)
    pin_name_match = re.search(r'PinName="([^"]+)"', line)
    if not pin_name_match:
        pin_name_match = re.search(r'PinName=([^,\s\)]+)', line)
        
    direction_match = re.search(r'Direction=([^,\s\)]+)', line)
    category_match = re.search(r'PinType\.PinCategory="?([^",\s\)]+)"?', line)
    
    linked_to = []
    linked_to_match = re.search(r'LinkedTo=\(([^\)]+)\)', line)
    if linked_to_match:
        conn_str = linked_to_match.group(1)
        conns = conn_str.split(',')
        for conn in conns:
            parts = conn.strip().split()
            if len(parts) >= 2:
                target_node = parts[0].strip('"')
                target_pin = parts[1].strip('"')
                linked_to.append((target_node, target_pin))
                
    return {
        "id": pin_id_match.group(1).strip('"') if pin_id_match else None,
        "name": pin_name_match.group(1) if pin_name_match else None,
        "direction": direction_match.group(1) if direction_match else "EGPD_Input",
        "category": category_match.group(1) if category_match else "struct",
        "linked_to": linked_to
    }

def parse_t3d(t3d_text: str):
    lines = t3d_text.splitlines()
    nodes = {}
    elements = []
    current_node = None
    in_object = 0
    
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("Begin Object"):
            if in_object == 0:
                match = re.search(r'Class=(\S+)\s+Name="([^"]+)"', stripped)
                if not match:
                    match = re.search(r'Class=(\S+)\s+Name=(\S+)', stripped)
                if match:
                    class_name = match.group(1)
                    node_name = match.group(2).strip('"')
                    current_node = {
                        "name": node_name,
                        "class": class_name,
                        "x": 0,
                        "y": 0,
                        "pins": [],
                        "lines": []
                    }
                    elements.append(current_node)
            in_object += 1
            
        if in_object > 0:
            current_node["lines"].append(line)
            if stripped.startswith("NodePosX="):
                try:
                    current_node["x"] = int(stripped.split("=")[1])
                except Exception:
                    pass
            elif stripped.startswith("NodePosY="):
                try:
                    current_node["y"] = int(stripped.split("=")[1])
                except Exception:
                    pass
            elif stripped.startswith("CustomProperties Pin"):
                pin_data = parse_pin_line(stripped)
                if pin_data:
                    current_node["pins"].append(pin_data)
        else:
            elements.append(line)
            
        if stripped.startswith("End Object"):
            in_object -= 1
            if in_object == 0:
                nodes[current_node["name"]] = current_node
                current_node = None
                
    return nodes, elements

def layout_blueprint_graph(nodes: dict) -> dict:
    exec_edges = {}
    exec_inputs = {name: 0 for name in nodes}
    data_dependencies = {name: [] for name in nodes}
    
    for name, node in nodes.items():
        for pin in node["pins"]:
            is_exec = pin["category"] == "exec"
            is_output = pin["direction"] == "EGPD_Output"
            
            for target_node, target_pin in pin["linked_to"]:
                if target_node not in nodes:
                    continue
                if is_exec:
                    if is_output:
                        exec_edges.setdefault(name, []).append(target_node)
                        exec_inputs[target_node] += 1
                else:
                    if not is_output:
                        data_dependencies[name].append(target_node)
                        
    exec_roots = []
    for name, node in nodes.items():
        has_exec_pins = any(p["category"] == "exec" for p in node["pins"])
        if has_exec_pins and exec_inputs[name] == 0:
            exec_roots.append(name)
            
    visited = set()
    positions = {}
    
    exec_spacing_x = 350
    exec_spacing_y = 200
    data_spacing_x = 250
    data_spacing_y = 150
    
    current_root_y = 0
    for root in exec_roots:
        if root in visited:
            continue
            
        def layout_exec_tree(node_name, x, y):
            if node_name in visited:
                return y
            
            visited.add(node_name)
            positions[node_name] = (x, y)
            
            next_nodes = exec_edges.get(node_name, [])
            if not next_nodes:
                return y
                
            if len(next_nodes) == 1:
                return layout_exec_tree(next_nodes[0], x + exec_spacing_x, y)
            
            max_y = y
            for i, next_node in enumerate(next_nodes):
                branch_y = max_y + (i * exec_spacing_y)
                max_y = max(max_y, layout_exec_tree(next_node, x + exec_spacing_x, branch_y))
            return max_y
            
        root_max_y = layout_exec_tree(root, 0, current_root_y)
        current_root_y = root_max_y + 300
        
    # Fallback for execution nodes that are part of a cycle (no root)
    for name, node in nodes.items():
        if name not in visited and any(p["category"] == "exec" for p in node["pins"]):
            root_max_y = layout_exec_tree(name, 0, current_root_y)
            current_root_y = root_max_y + 300
            
    def layout_data_deps(node_name):
        if node_name not in positions:
            return
        x, y = positions[node_name]
        deps = data_dependencies.get(node_name, [])
        
        for i, dep in enumerate(deps):
            if dep in visited:
                continue
                
            dep_x = x - data_spacing_x
            dep_y = y + (i * data_spacing_y)
            visited.add(dep)
            positions[dep] = (dep_x, dep_y)
            layout_data_deps(dep)
            
    positioned_nodes = list(positions.keys())
    for node_name in positioned_nodes:
        layout_data_deps(node_name)
        
    remaining = [name for name in nodes if name not in visited]
    if remaining:
        grid_cols = 4
        start_y = current_root_y + 200
        for idx, name in enumerate(remaining):
            col = idx % grid_cols
            row = idx // grid_cols
            positions[name] = (col * 300, start_y + row * 150)
            visited.add(name)
            
    return positions

def rebuild_t3d(elements: list, positions: dict) -> str:
    result_lines = []
    for elem in elements:
        if isinstance(elem, str):
            result_lines.append(elem)
        else:
            name = elem["name"]
            pos_x, pos_y = positions.get(name, (elem["x"], elem["y"]))
            
            new_node_lines = []
            has_pos_x = False
            has_pos_y = False
            
            for line in elem["lines"]:
                stripped = line.strip()
                if stripped.startswith("NodePosX="):
                    indent = line[:line.find("NodePosX=")]
                    new_node_lines.append(f"{indent}NodePosX={pos_x}")
                    has_pos_x = True
                elif stripped.startswith("NodePosY="):
                    indent = line[:line.find("NodePosY=")]
                    new_node_lines.append(f"{indent}NodePosY={pos_y}")
                    has_pos_y = True
                else:
                    new_node_lines.append(line)
                    
            if not has_pos_y:
                for idx, line in enumerate(new_node_lines):
                    if line.strip().startswith("Begin Object"):
                        indent = line[:line.find("Begin Object")] + "   "
                        new_node_lines.insert(idx + 1, f"{indent}NodePosY={pos_y}")
                        break
            if not has_pos_x:
                for idx, line in enumerate(new_node_lines):
                    if line.strip().startswith("Begin Object"):
                        indent = line[:line.find("Begin Object")] + "   "
                        new_node_lines.insert(idx + 1, f"{indent}NodePosX={pos_x}")
                        break
                        
            result_lines.extend(new_node_lines)
            
    return "\n".join(result_lines)

def format_layout(t3d_text: str) -> str:
    try:
        nodes, elements = parse_t3d(t3d_text)
        if not nodes:
            return t3d_text
        positions = layout_blueprint_graph(nodes)
        return rebuild_t3d(elements, positions)
    except Exception as e:
        logger.error(f"Error auto-formatting T3D layout: {e}", exc_info=True)
        return t3d_text
