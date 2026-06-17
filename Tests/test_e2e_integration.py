import pytest
import json

def test_python_mcp_query_cpp_ast(mock_agent_client):
    """
    Test Python MCP tool: query_cpp_ast
    """
    response = mock_agent_client.call_python_tool(
        "query_cpp_ast",
        {"query": "class AMorphTargetActor"}
    )
    assert response is not None
    assert "result" in response
    result = response["result"]
    assert "content" in result
    assert len(result["content"]) > 0
    text_content = result["content"][0]["text"]
    assert "query_cpp_ast" in text_content
    assert "AMorphTargetActor" in text_content

def test_python_mcp_generate_compile_commands(mock_agent_client):
    """
    Test Python MCP tool: generate_compile_commands
    """
    response = mock_agent_client.call_python_tool(
        "generate_compile_commands",
        {}
    )
    assert response is not None
    assert "result" in response
    result = response["result"]
    assert "content" in result
    assert len(result["content"]) > 0
    assert "generate_compile_commands" in result["content"][0]["text"]

def test_python_mcp_search_vector_db(mock_agent_client):
    """
    Test Python MCP tool: search_vector_db
    """
    response = mock_agent_client.call_python_tool(
        "search_vector_db",
        {"query": "character movement replication"}
    )
    assert response is not None
    assert "result" in response
    result = response["result"]
    assert "content" in result
    assert len(result["content"]) > 0
    assert "search_vector_db" in result["content"][0]["text"]

def test_cpp_mcp_get_blueprint_schema(mock_agent_client):
    """
    Test C++ HTTP tool: get_blueprint_schema
    """
    response = mock_agent_client.call_cpp_tool(
        "get_blueprint_schema",
        {"asset_path": "/Game/Blueprint/Player/BP_RoundPawn"}
    )
    assert response is not None
    assert response.get("bSuccess") is True
    result_msg = response.get("ResultMessage", "")
    schema = json.loads(result_msg)
    for key in ["asset_name", "parent_class", "variables", "custom_events"]:
        assert key in schema

def test_cpp_mcp_inject_blueprint_nodes_t3d(mock_agent_client):
    """
    Test C++ HTTP tool: inject_blueprint_nodes_t3d
    """
    valid_t3d = (
        "Begin Object Class=/Script/BlueprintGraph.K2Node_VariableGet Name=\"K2Node_VariableGet_0\"\n"
        "   NodeGuid=GUID_Node1\n"
        "   CustomProperties Pin (PinId=LINK_1,PinName=\"Output\",Direction=\"EGPD_Output\")\n"
        "End Object"
    )
    response = mock_agent_client.call_cpp_tool(
        "inject_blueprint_nodes_t3d",
        {
            "asset_path": "/Game/Blueprint/Player/BP_RoundPawn",
            "t3d_text": valid_t3d
        }
    )
    assert response is not None
    assert response.get("bSuccess") is True
    assert "Injected" in response.get("ResultMessage", "")

def test_cpp_mcp_get_cpp_reflection_info(mock_agent_client):
    """
    Test C++ HTTP tool: get_cpp_reflection_info
    """
    response = mock_agent_client.call_cpp_tool(
        "get_cpp_reflection_info",
        {"class_name": "Actor"}
    )
    assert response is not None
    assert response.get("bSuccess") is True
    result_msg = response.get("ResultMessage", "")
    info = json.loads(result_msg)
    for key in ["class_name", "parent_class", "properties", "functions"]:
        assert key in info

