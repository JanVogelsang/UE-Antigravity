import pytest
import json

def test_set_niagara_parameter_create_system(mock_agent_client):
    """
    Ensure test system /Game/Effects/NS_TestParameterSystem exists or create it.
    """
    res = mock_agent_client.call_cpp_tool(
        "create_niagara_system",
        {"asset_path": "/Game/Effects/NS_TestParameterSystem"}
    )
    # create_niagara_system will succeed or fail if already existing/created, but system should exist
    assert res is not None
    assert "bSuccess" in res

def test_set_niagara_parameter_float_snake_and_pascal(mock_agent_client):
    """
    Test setting Float parameter using both snake_case and PascalCase aliases.
    """
    system_path = "/Game/Effects/NS_TestParameterSystem"
    
    # 1. snake_case
    res1 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": system_path,
            "parameter_scope": "User",
            "parameter_name": "SpawnRate",
            "data_type": "Float",
            "value": 150.5
        }
    )
    assert res1 is not None
    assert res1.get("bSuccess") is True, f"Failed snake_case float: {res1.get('Errors')}"
    assert "User.SpawnRate" in res1.get("ResultMessage", "")

    # 2. PascalCase
    res2 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "SystemAsset": system_path,
            "ParameterScope": "User",
            "ParameterName": "SpawnRate",
            "DataType": "Float",
            "Value": 250.0
        }
    )
    assert res2 is not None
    assert res2.get("bSuccess") is True, f"Failed PascalCase float: {res2.get('Errors')}"
    assert "User.SpawnRate" in res2.get("ResultMessage", "")

def test_set_niagara_parameter_vector2(mock_agent_client):
    """
    Test setting Vector2 parameter with object, array, string, and scalar formats.
    """
    system_path = "/Game/Effects/NS_TestParameterSystem"

    # Object format
    res1 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": system_path,
            "parameter_name": "Scale2D",
            "data_type": "Vector2",
            "value": {"x": 2.5, "y": 5.0}
        }
    )
    assert res1.get("bSuccess") is True, f"Failed Vector2 object: {res1.get('Errors')}"

    # Array format
    res2 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": system_path,
            "parameter_name": "Scale2D",
            "data_type": "Vector2",
            "value": [3.0, 6.0]
        }
    )
    assert res2.get("bSuccess") is True, f"Failed Vector2 array: {res2.get('Errors')}"

    # String comma separated
    res3 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": system_path,
            "parameter_name": "Scale2D",
            "data_type": "Vector2",
            "value": "1.5,4.5"
        }
    )
    assert res3.get("bSuccess") is True, f"Failed Vector2 string: {res3.get('Errors')}"

def test_set_niagara_parameter_vector3(mock_agent_client):
    """
    Test setting Vector3 parameter with object and array formats.
    """
    system_path = "/Game/Effects/NS_TestParameterSystem"

    # Object format
    res1 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": system_path,
            "parameter_name": "Velocity3D",
            "data_type": "Vector3",
            "value": {"x": 100.0, "y": 0.0, "z": -980.0}
        }
    )
    assert res1.get("bSuccess") is True, f"Failed Vector3 object: {res1.get('Errors')}"

    # Array format
    res2 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "SystemAsset": system_path,
            "ParameterName": "Velocity3D",
            "DataType": "Vector3",
            "Value": [0.0, 500.0, 200.0]
        }
    )
    assert res2.get("bSuccess") is True, f"Failed Vector3 array: {res2.get('Errors')}"

def test_set_niagara_parameter_linear_color(mock_agent_client):
    """
    Test setting LinearColor parameter with RGBA object and array formats.
    """
    system_path = "/Game/Effects/NS_TestParameterSystem"

    # Object format
    res1 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": system_path,
            "parameter_name": "PrimaryColor",
            "data_type": "LinearColor",
            "value": {"r": 1.0, "g": 0.2, "b": 0.1, "a": 1.0}
        }
    )
    assert res1.get("bSuccess") is True, f"Failed LinearColor object: {res1.get('Errors')}"

    # Array format
    res2 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "SystemAsset": system_path,
            "ParameterName": "PrimaryColor",
            "DataType": "LinearColor",
            "Value": [0.0, 1.0, 0.5, 0.8]
        }
    )
    assert res2.get("bSuccess") is True, f"Failed LinearColor array: {res2.get('Errors')}"

def test_set_niagara_parameter_bool_and_int(mock_agent_client):
    """
    Test setting Bool and Int32 parameters.
    """
    system_path = "/Game/Effects/NS_TestParameterSystem"

    # Bool
    res_bool = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": system_path,
            "parameter_name": "bEnabled",
            "data_type": "Bool",
            "value": True
        }
    )
    assert res_bool.get("bSuccess") is True, f"Failed Bool parameter: {res_bool.get('Errors')}"

    # Int32
    res_int = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": system_path,
            "parameter_name": "ParticleCount",
            "data_type": "Int32",
            "value": 500
        }
    )
    assert res_int.get("bSuccess") is True, f"Failed Int32 parameter: {res_int.get('Errors')}"

def test_set_niagara_parameter_curve_float(mock_agent_client):
    """
    Test setting CurveFloat parameter with keyframe insertion.
    """
    system_path = "/Game/Effects/NS_TestParameterSystem"

    res = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": system_path,
            "parameter_name": "FloatCurveOverLife",
            "data_type": "CurveFloat",
            "curve_keys": [
                {"time": 0.0, "value": 0.0},
                {"time": 0.5, "value": 1.5},
                {"time": 1.0, "value": 0.0}
            ]
        }
    )
    assert res.get("bSuccess") is True, f"Failed CurveFloat parameter: {res.get('Errors')}"

    # Also test PascalCase CurveKeys alias
    res_pascal = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "SystemAsset": system_path,
            "ParameterName": "FloatCurveOverLife",
            "DataType": "CurveFloat",
            "CurveKeys": [
                {"Time": 0.0, "Value": 1.0},
                {"Time": 1.0, "Value": 0.1}
            ]
        }
    )
    assert res_pascal.get("bSuccess") is True, f"Failed CurveFloat PascalCase: {res_pascal.get('Errors')}"

def test_set_niagara_parameter_curve_linear_color(mock_agent_client):
    """
    Test setting CurveLinearColor parameter with RGBA keyframes.
    """
    system_path = "/Game/Effects/NS_TestParameterSystem"

    # RGBA object keyframes
    res1 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": system_path,
            "parameter_name": "ColorCurveOverLife",
            "data_type": "CurveLinearColor",
            "curve_keys": [
                {"time": 0.0, "r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0},
                {"time": 0.5, "r": 0.0, "g": 1.0, "b": 0.0, "a": 1.0},
                {"time": 1.0, "r": 0.0, "g": 0.0, "b": 1.0, "a": 1.0}
            ]
        }
    )
    assert res1.get("bSuccess") is True, f"Failed CurveLinearColor RGBA: {res1.get('Errors')}"

    # Array value keyframes
    res2 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "SystemAsset": system_path,
            "ParameterName": "ColorCurveOverLife",
            "DataType": "CurveLinearColor",
            "CurveKeys": [
                {"Time": 0.0, "Value": [1.0, 1.0, 0.0, 1.0]},
                {"Time": 1.0, "Value": [0.0, 0.0, 0.0, 0.0]}
            ]
        }
    )
    assert res2.get("bSuccess") is True, f"Failed CurveLinearColor Array Value: {res2.get('Errors')}"

def test_set_niagara_parameter_missing_and_invalid(mock_agent_client):
    """
    Test error handling for non-existent asset, missing parameter name, and unsupported type.
    """
    # 1. Non-existent system path
    res1 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": "/Game/Effects/NS_NonExistentSystem_12345",
            "parameter_name": "TestParam",
            "data_type": "Float",
            "value": 1.0
        }
    )
    assert res1.get("bSuccess") is False
    assert len(res1.get("Errors", [])) > 0

    # 2. Missing parameter name
    res2 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": "/Game/Effects/NS_TestParameterSystem",
            "data_type": "Float",
            "value": 1.0
        }
    )
    assert res2.get("bSuccess") is False

    # 3. Unsupported data type
    res3 = mock_agent_client.call_cpp_tool(
        "set_niagara_parameter",
        {
            "system_path": "/Game/Effects/NS_TestParameterSystem",
            "parameter_name": "TestParam",
            "data_type": "UnsupportedType123",
            "value": 1.0
        }
    )
    assert res3.get("bSuccess") is False
    assert any("Unsupported" in err for err in res3.get("Errors", []))
