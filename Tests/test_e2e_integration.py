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
    assert "Compilation generation started in background" in result["content"][0]["text"]

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


@pytest.fixture(scope="module")
def pie_session(mock_agent_client):
    import time
    # Start PIE session
    res = mock_agent_client.call_cpp_tool("start_pie_session", {})
    assert res is not None
    assert res.get("bSuccess") is True
    # Wait for the session and viewport to initialize
    start_time = time.time()
    pie_ok = False
    while time.time() - start_time < 15.0:
        res_state = mock_agent_client.call_cpp_tool("query_world_state", {"classes": ["Actor"]})
        if res_state and res_state.get("bSuccess") is True:
            pie_ok = True
            break
        time.sleep(0.5)
    assert pie_ok, "PIE session failed to initialize and report world state within timeout."
    yield
    # Stop PIE session
    mock_agent_client.call_cpp_tool("stop_pie_session", {})
    time.sleep(1.0)


def test_cpp_mcp_extract_ui_state(mock_agent_client, pie_session):
    """
    Test C++ HTTP tool: extract_ui_state
    """
    response = mock_agent_client.call_cpp_tool(
        "extract_ui_state",
        {}
    )
    assert response is not None
    assert response.get("bSuccess") is True
    result_msg = response.get("ResultMessage", "")
    ui_state = json.loads(result_msg)
    assert "umg" in ui_state
    assert "slate" in ui_state


def test_cpp_mcp_trigger_ui_element(mock_agent_client, pie_session):
    """
    Test C++ HTTP tool: trigger_ui_element
    """
    # First extract current UI layout to find a valid widget
    response = mock_agent_client.call_cpp_tool("extract_ui_state", {})
    assert response is not None
    assert response.get("bSuccess") is True
    ui_state = json.loads(response.get("ResultMessage", ""))
    
    widget_path = None
    if ui_state.get("umg"):
        widget_path = list(ui_state["umg"].keys())[0]
    elif ui_state.get("slate"):
        widget_path = list(ui_state["slate"].keys())[0]

    # If any interactable widget is active in viewport, trigger programmatic click on it
    if widget_path:
        response = mock_agent_client.call_cpp_tool(
            "trigger_ui_element",
            {"widget_path": widget_path}
        )
        assert response is not None
        assert response.get("bSuccess") is True
        assert "clicked widget" in response.get("ResultMessage", "").lower()


def test_cpp_mcp_query_world_state(mock_agent_client, pie_session):
    """
    Test C++ HTTP tool: query_world_state
    """
    # Query with generic 'Actor' class filter which always matches loaded level actors
    response = mock_agent_client.call_cpp_tool(
        "query_world_state",
        {"classes": ["Actor"]}
    )
    assert response is not None
    assert response.get("bSuccess") is True
    result_msg = response.get("ResultMessage", "")
    world_state = json.loads(result_msg)
    assert "player" in world_state
    assert "actors" in world_state


def test_cpp_mcp_execute_python_script_validation(mock_agent_client):
    """
    Test that execute_python_script fails validation under various invalid justifications
    and succeeds under a valid justification.
    """
    # 1. Missing justification
    response = mock_agent_client.call_cpp_tool(
        "execute_python_script",
        {
            "script": "print('hello')"
        }
    )
    assert response is not None
    assert response.get("bSuccess") is False
    assert "Missing or empty required field" in "".join(response.get("Errors", []))

    # 2. Justification too short (< 10 chars)
    response = mock_agent_client.call_cpp_tool(
        "execute_python_script",
        {
            "script": "print('hello')",
            "justification_why_native_tools_or_skills_are_insufficient": "too short"
        }
    )
    assert response is not None
    assert response.get("bSuccess") is False
    assert "too short" in "".join(response.get("Errors", []))

    # 3. Justification too long (> 1000 chars)
    long_justification = "a" * 1001
    response = mock_agent_client.call_cpp_tool(
        "execute_python_script",
        {
            "script": "print('hello')",
            "justification_why_native_tools_or_skills_are_insufficient": long_justification
        }
    )
    assert response is not None
    assert response.get("bSuccess") is False
    assert "too long" in "".join(response.get("Errors", []))

    # 4. Justification too few words (< 4 words)
    response = mock_agent_client.call_cpp_tool(
        "execute_python_script",
        {
            "script": "print('hello')",
            "justification_why_native_tools_or_skills_are_insufficient": "No native tools"
        }
    )
    assert response is not None
    assert response.get("bSuccess") is False
    assert "too vague" in "".join(response.get("Errors", []))

    # 5. Low-effort phrase check
    response = mock_agent_client.call_cpp_tool(
        "execute_python_script",
        {
            "script": "print('hello')",
            "justification_why_native_tools_or_skills_are_insufficient": "Testing python execution now"
        }
    )
    assert response is not None
    assert response.get("bSuccess") is False
    assert "flagged as a low-effort placeholder" in "".join(response.get("Errors", []))

    # 6. Valid justification
    response = mock_agent_client.call_cpp_tool(
        "execute_python_script",
        {
            "script": "print('hello')",
            "justification_why_native_tools_or_skills_are_insufficient": "We need to run custom Python reflection because no native tool can read metadata of non-blueprint UObjects"
        }
    )
    assert response is not None
    errors_str = "".join(response.get("Errors", []))
    assert "justification_why_native_tools_or_skills_are_insufficient" not in errors_str
    assert "too short" not in errors_str
    assert "too long" not in errors_str
    assert "too vague" not in errors_str
    assert "placeholder" not in errors_str
