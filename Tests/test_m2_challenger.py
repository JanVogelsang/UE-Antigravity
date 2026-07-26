import pytest
import json
import re

def test_get_cpp_reflection_info_actor_with_and_without_prefix(mock_agent_client):
    """
    Test querying class 'Actor' with and without 'A' prefix.
    """
    # 1. Query without prefix: Actor (should work on both mock and real)
    res_without = mock_agent_client.call_cpp_tool(
        "get_cpp_reflection_info",
        {"class_name": "Actor"}
    )
    assert res_without is not None
    assert res_without.get("bSuccess") is True, f"Failed: {res_without.get('Errors')}"
    info_without = json.loads(res_without["ResultMessage"])
    assert info_without["class_name"] == "Actor"

    # 2. Query with 'A' prefix: AActor
    res_with = mock_agent_client.call_cpp_tool(
        "get_cpp_reflection_info",
        {"class_name": "AActor"}
    )
    assert res_with is not None
    assert res_with.get("bSuccess") is True, f"Failed: {res_with.get('Errors')}"
    info_with = json.loads(res_with["ResultMessage"])
    assert info_with["class_name"] == "Actor"

def test_get_cpp_reflection_info_nonexistent_class(mock_agent_client):
    """
    Test querying a non-existent class and verifying failure behavior.
    """
    res = mock_agent_client.call_cpp_tool(
        "get_cpp_reflection_info",
        {"class_name": "NonExistentClass"}
    )
    assert res is not None
    assert res.get("bSuccess") is False
    errors = res.get("Errors", [])
    assert len(errors) > 0
    assert any("not found" in err.lower() or "not exist" in err.lower() for err in errors)

def test_get_cpp_reflection_info_disabled_fields(mock_agent_client):
    """
    Test disabling properties, functions, interfaces, or metadata extraction (setting fields to false)
    and verifying that those fields are empty or not present in the output.
    """
    # 1. Get base case (all enabled) to see what is normally present
    res_all = mock_agent_client.call_cpp_tool(
        "get_cpp_reflection_info",
        {"class_name": "Actor"}
    )
    assert res_all.get("bSuccess") is True
    info_all = json.loads(res_all["ResultMessage"])

    # 2. Test include_properties = False
    res_no_prop = mock_agent_client.call_cpp_tool(
        "get_cpp_reflection_info",
        {
            "class_name": "Actor",
            "include_properties": False
        }
    )
    assert res_no_prop.get("bSuccess") is True
    info_no_prop = json.loads(res_no_prop["ResultMessage"])
    assert "properties" not in info_no_prop
    if "functions" in info_all:
        assert "functions" in info_no_prop
    if "interfaces" in info_all:
        assert "interfaces" in info_no_prop
    if "metadata" in info_all:
        assert "metadata" in info_no_prop

    # 3. Test include_functions = False
    res_no_func = mock_agent_client.call_cpp_tool(
        "get_cpp_reflection_info",
        {
            "class_name": "Actor",
            "include_functions": False
        }
    )
    assert res_no_func.get("bSuccess") is True
    info_no_func = json.loads(res_no_func["ResultMessage"])
    assert "functions" not in info_no_func

    # 4. Test include_interfaces = False
    res_no_inter = mock_agent_client.call_cpp_tool(
        "get_cpp_reflection_info",
        {
            "class_name": "Actor",
            "include_interfaces": False
        }
    )
    assert res_no_inter.get("bSuccess") is True
    info_no_inter = json.loads(res_no_inter["ResultMessage"])
    assert "interfaces" not in info_no_inter

    # 5. Test include_metadata = False
    res_no_meta = mock_agent_client.call_cpp_tool(
        "get_cpp_reflection_info",
        {
            "class_name": "Actor",
            "include_metadata": False
        }
    )
    assert res_no_meta.get("bSuccess") is True
    info_no_meta = json.loads(res_no_meta["ResultMessage"])
    assert "metadata" not in info_no_meta
    # Verify that metadata is also omitted inside properties and functions if they are present
    if "properties" in info_no_meta:
        for prop in info_no_meta["properties"]:
            assert "metadata" not in prop
    if "functions" in info_no_meta:
        for func in info_no_meta["functions"]:
            assert "metadata" not in func

def test_get_cpp_reflection_info_validate_structures(mock_agent_client):
    """
    Validate that all properties, functions, and interfaces are returned as valid JSON arrays/objects.
    """
    res = mock_agent_client.call_cpp_tool(
        "get_cpp_reflection_info",
        {"class_name": "Actor"}
    )
    assert res.get("bSuccess") is True
    info = json.loads(res["ResultMessage"])
    
    assert "class_name" in info
    assert "parent_class" in info
    assert isinstance(info.get("is_abstract"), bool)
    assert isinstance(info.get("is_blueprint_spawnable"), bool)
    
    if "metadata" in info:
        assert isinstance(info["metadata"], dict)
        
    if "interfaces" in info:
        assert isinstance(info["interfaces"], list)
        for val in info["interfaces"]:
            assert isinstance(val, str)
            
    if "properties" in info:
        assert isinstance(info["properties"], list)
        for prop in info["properties"]:
            assert isinstance(prop, dict)
            assert "name" in prop
            assert "type" in prop
            assert isinstance(prop.get("flags"), list)
            if "metadata" in prop:
                assert isinstance(prop["metadata"], dict)
                
    if "functions" in info:
        assert isinstance(info["functions"], list)
        for func in info["functions"]:
            assert isinstance(func, dict)
            assert "name" in func
            assert isinstance(func.get("is_pure"), bool)
            assert isinstance(func.get("flags"), list)
            assert isinstance(func.get("parameters"), list)
            for param in func["parameters"]:
                assert isinstance(param, dict)
                assert "name" in param
                assert "type" in param
                assert isinstance(param.get("flags"), list)
            if "metadata" in func:
                assert isinstance(func["metadata"], dict)

def test_inject_blueprint_nodes_t3d_guid_collision_prevention(mock_agent_client):
    """
    Validate GUID resolution prefix collision prevention. Send a mock T3D string containing
    LINK_1, LINK_10, LINK_100, GUID_Node1, GUID_Node10 and assert that the resolved string
    does not contain any of the placeholder names, and that no two different placeholders
    resolved to the same value, and that LINK_10 did not get corrupted to a 33-char prefix-matching string.
    """
    mock_t3d = (
        "Begin Object Class=/Script/BlueprintGraph.K2Node_VariableGet Name=\"K2Node_VariableGet_0\"\n"
        "   NodeGuid=GUID_Node1\n"
        "   CustomProperties Pin (PinId=LINK_1,PinName=\"Output\",Direction=\"EGPD_Output\")\n"
        "End Object\n"
        "Begin Object Class=/Script/BlueprintGraph.K2Node_VariableGet Name=\"K2Node_VariableGet_1\"\n"
        "   NodeGuid=GUID_Node10\n"
        "   CustomProperties Pin (PinId=LINK_10,PinName=\"Output\",Direction=\"EGPD_Output\")\n"
        "End Object\n"
        "Begin Object Class=/Script/BlueprintGraph.K2Node_VariableGet Name=\"K2Node_VariableGet_2\"\n"
        "   NodeGuid=GUID_Node100\n"
        "   CustomProperties Pin (PinId=LINK_100,PinName=\"Output\",Direction=\"EGPD_Output\")\n"
        "End Object"
    )
    
    # We target BP_RoundPawn because it is set up in the testing workspace
    res = mock_agent_client.call_cpp_tool(
        "inject_blueprint_nodes_t3d",
        {
            "asset_path": "/Game/Blueprint/Player/BP_RoundPawn",
            "t3d_text": mock_t3d
        }
    )
    assert res is not None
    assert res.get("bSuccess") is True
    
    resolved_t3d = res["ResultMessage"]
    
    # 1. Assert resolved string does not contain any placeholder names
    placeholders = ["LINK_1", "LINK_10", "LINK_100", "GUID_Node1", "GUID_Node10"]
    for p in placeholders:
        assert p not in resolved_t3d
        
    # 2. Verify length of every PinId and NodeGuid in resolved T3D text
    # (they should be 32-character hex GUIDs)
    pin_ids = re.findall(r'PinId=([A-Za-z0-9]+)', resolved_t3d)
    assert len(pin_ids) > 0, "No PinId found in resolved T3D text."
    for pid in pin_ids:
        assert len(pid) == 32, f"PinId '{pid}' has invalid length (corrupted?)"
        
    node_guids = re.findall(r'NodeGuid=([A-Za-z0-9]+)', resolved_t3d)
    assert len(node_guids) > 0, "No NodeGuid found in resolved T3D text."
    for ng in node_guids:
        assert len(ng) == 32, f"NodeGuid '{ng}' has invalid length"
        
    # 3. Assert no two different placeholders resolved to the same value
    all_resolved = pin_ids + node_guids
    assert len(all_resolved) == len(set(all_resolved)), "Some placeholders resolved to duplicate values!"
