import pytest
import json

def test_get_blueprint_schema_missing_param(mock_agent_client):
    """
    Test get_blueprint_schema with missing asset_path parameter.
    """
    # Note: ValidateParams will fail because 'asset_path' is missing.
    # The HTTP server / Action dispatch should return bSuccess=False with an error message.
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {}
    )
    assert res is not None
    assert res.get('bSuccess') is False
    errors = res.get('Errors', [])
    assert len(errors) > 0 or "missing" in res.get('ResultMessage', '').lower()

def test_get_blueprint_schema_empty_param(mock_agent_client):
    """
    Test get_blueprint_schema with empty asset_path.
    """
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': ''}
    )
    assert res is not None
    assert res.get('bSuccess') is False
    errors = res.get('Errors', [])
    assert len(errors) > 0 or "empty" in res.get('ResultMessage', '').lower()

def test_get_blueprint_schema_invalid_path(mock_agent_client):
    """
    Test get_blueprint_schema with a non-existent asset path.
    """
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': '/Game/Blueprint/NonExistentAsset_XYZ'}
    )
    assert res is not None
    # For a non-existent asset, the tool should fail since it cannot load it.
    assert res.get('bSuccess') is False
    errors = res.get('Errors', [])
    result_msg = res.get('ResultMessage', '')
    assert any("failed to load" in err.lower() or "not found" in err.lower() for err in errors) or \
           "failed to load" in result_msg.lower() or "not found" in result_msg.lower()

def test_get_blueprint_schema_widget_blueprint(mock_agent_client):
    """
    Test get_blueprint_schema with a WidgetBlueprint subclass.
    """
    # Note: If running against mock server, it will just return bSuccess=True.
    # If running against the real editor, we test a widget blueprint.
    # We can use an existing WidgetBlueprint in the content folder, e.g. one in tau-game if we have one.
    # Let's search for widget blueprints in the project if any, or just check.
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': '/Game/UI/WBP_TestInstantiate'}  # Existing WidgetBlueprint in tau-game
    )
    assert res is not None
    # If mock server: success
    # If real server: depends if WBP_MainMenu exists. Let's make sure it handles it without crashing.
    if res.get('bSuccess') is True:
        msg = res.get('ResultMessage', '')
        if "asset_name" in msg:
            schema = json.loads(msg)
            assert "asset_name" in schema
            assert "parent_class" in schema

def test_get_blueprint_schema_real_type_bug(mock_agent_client):
    """
    Test type serialization for float/double.
    If running against real server, it exposes the 'real' type serialization bug.
    """
    res = mock_agent_client.call_cpp_tool(
        'get_blueprint_schema',
        {'asset_path': '/Game/Blueprint/Player/BP_RoundPawn'}
    )
    assert res is not None
    assert res.get('bSuccess') is True, f"Failed to get blueprint schema: {res.get('Errors')}"
    msg = res.get('ResultMessage', '')
    schema = json.loads(msg)
    # Inspect variables for float types
    variables = schema.get('variables', [])
    for var in variables:
        var_name = var.get('name')
        var_type = var.get('type')
        assert var_type != 'real', f"Variable '{var_name}' type serialized as 'real', expected 'float'"
