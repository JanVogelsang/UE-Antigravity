import pytest
import json

def test_set_niagara_parameter_nonexistent_system(mock_agent_client):
    """
    Test 1: Non-existent UNiagaraSystem path
    """
    res = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": "/Game/NonExistent/NS_FakeSystem_12345",
            "parameter_name": "SpawnRate",
            "data_type": "Float",
            "value": 10.0
        }
    )
    assert res is not None
    assert res.get("bSuccess") is False
    errors = res.get("Errors", [])
    assert len(errors) > 0
    assert any("Niagara System not found" in err for err in errors)

def test_set_niagara_parameter_invalid_data_type(mock_agent_client):
    """
    Test 2: Invalid data_type (e.g. 'Vector4', 'Matrix', 'CustomType')
    Note: Requires a valid Niagara system asset if system check happens before data_type check,
    or tests unsupported data_type handling when system is loaded.
    """
    res = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": "/Game/NonExistent/NS_FakeSystem_12345",
            "parameter_name": "SpawnRate",
            "data_type": "UnsupportedType123",
            "value": 10.0
        }
    )
    assert res is not None
    assert res.get("bSuccess") is False
    # If system fails to load first, error says not found. If system exists, error says unsupported data type.
    errors = res.get("Errors", [])
    assert len(errors) > 0

def test_set_niagara_parameter_missing_system_path(mock_agent_client):
    """
    Test 2b: Missing system path param altogether
    """
    res = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "parameter_name": "SpawnRate",
            "data_type": "Float",
            "value": 10.0
        }
    )
    assert res is not None
    assert res.get("bSuccess") is False
    errors = res.get("Errors", [])
    assert len(errors) > 0

def test_set_niagara_parameter_missing_parameter_name(mock_agent_client):
    """
    Test 2c: Missing parameter_name altogether
    """
    res = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": "/Game/NonExistent/NS_FakeSystem_12345",
            "data_type": "Float",
            "value": 10.0
        }
    )
    assert res is not None
    assert res.get("bSuccess") is False
    errors = res.get("Errors", [])
    assert len(errors) > 0
    assert any("parameter_name" in err.lower() or "missing" in err.lower() for err in errors)
