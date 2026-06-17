import pytest
import json

@pytest.fixture(autouse=True)
def skip_on_live(is_live_editor):
    if is_live_editor:
        pytest.skip("Skipping mock-only tests on live editor.")

def test_get_blueprint_schema_success(mock_agent_client):
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': '/Game/Blueprints/BP_MyCharacter'}
    )
    assert res is not None
    assert res.get('bSuccess') is True
    
    result_msg = res.get('ResultMessage', '')
    schema = json.loads(result_msg)
    assert schema.get("asset_name") == "BP_MyCharacter"
    assert schema.get("parent_class") in ["Character", "Actor"]
    assert isinstance(schema.get("variables"), list)
    assert isinstance(schema.get("custom_events"), list)
    for key in ["asset_name", "parent_class", "variables", "custom_events"]:
        assert key in schema

def test_get_blueprint_schema_missing_params(mock_agent_client):
    # Test case 1: empty dict
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {}
    )
    assert res is not None
    assert res.get('bSuccess') is False
    assert any("missing" in err.lower() or "required" in err.lower() for err in res.get('Errors', []))

    # Test case 2: empty asset path
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': ''}
    )
    assert res is not None
    assert res.get('bSuccess') is False
    assert any("missing" in err.lower() or "empty" in err.lower() or "required" in err.lower() for err in res.get('Errors', []))

def test_get_blueprint_schema_nonexistent(mock_agent_client):
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': '/Game/NonExistentAsset'}
    )
    assert res is not None
    assert res.get('bSuccess') is False
    errors = res.get('Errors', [])
    assert any("not load" in err.lower() or "fallback" in err.lower() or "not found" in err.lower() for err in errors)

def test_get_blueprint_schema_non_blueprint(mock_agent_client):
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': '/Game/Meshes/SM_Chair'}
    )
    assert res is not None
    assert res.get('bSuccess') is False
    errors = res.get('Errors', [])
    assert any("not a blueprint" in err.lower() for err in errors)

def test_get_blueprint_schema_widget_blueprint(mock_agent_client):
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': '/Game/UI/WBP_MainMenu'}
    )
    assert res is not None
    assert res.get('bSuccess') is True
    
    result_msg = res.get('ResultMessage', '')
    schema = json.loads(result_msg)
    assert schema.get("asset_name") == "WBP_MainMenu"
    assert schema.get("parent_class") == "UserWidget"

def test_get_blueprint_schema_anim_blueprint(mock_agent_client):
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': '/Game/Animations/ABP_Mannequin'}
    )
    assert res is not None
    assert res.get('bSuccess') is True
    
    result_msg = res.get('ResultMessage', '')
    schema = json.loads(result_msg)
    assert schema.get("asset_name") == "ABP_Mannequin"
    assert schema.get("parent_class") == "AnimInstance"

def test_get_blueprint_schema_containers(mock_agent_client):
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': '/Game/Blueprints/BP_ContainerTest'}
    )
    assert res is not None
    assert res.get('bSuccess') is True
    
    result_msg = res.get('ResultMessage', '')
    schema = json.loads(result_msg)
    assert schema.get("asset_name") == "BP_ContainerTest"
    
    variables = schema.get("variables", [])
    assert len(variables) >= 3
    
    var_types = {v["name"]: v["type"] for v in variables}
    assert "TArray" in var_types.get("MyArray", "")
    assert "TSet" in var_types.get("MySet", "")
    assert "TMap" in var_types.get("MyMap", "")

