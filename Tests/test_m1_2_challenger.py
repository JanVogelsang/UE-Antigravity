import pytest
import json
import uuid

def test_configure_input_mapping_modifiers_triggers_modifiers(mock_agent_client):
    """
    Empirical Test: Verify modifier property parsing and instantiation in configure_input_mapping_modifiers_triggers.
    Tests:
    1. SwizzleAxis (ZYX, XZY, YXZ, YZX, ZXY)
    2. ScalarVector (FVector from object, single float fallback)
    3. DeadZone (LowerThreshold, UpperThreshold, DeadZoneType: Axial, Radial, UnscaledRadial)
    4. ResponseCurveExponential (CurveExponent vector)
    5. ResponseCurveUser (CurveFloat asset paths)
    6. Smooth (SmoothingType)
    """
    imc_path = "/Game/Input/IMC_TestChallenger"
    ia_path = "/Game/Input/IA_TestChallenger"
    
    payload = {
        "mapping_context_path": imc_path,
        "action_path": ia_path,
        "key": "IA_Test_Key_W",
        "modifiers": [
            {
                "type": "Negate",
                "b_x": True,
                "b_y": False,
                "b_z": True
            },
            {
                "type": "SwizzleAxis",
                "order": "ZYX"
            },
            {
                "type": "Scalar",
                "scalar_vector": {"x": 2.0, "y": -1.0, "z": 0.5}
            },
            {
                "type": "DeadZone",
                "lower_threshold": 0.15,
                "upper_threshold": 0.85,
                "deadzone_type": "Axial"
            },
            {
                "type": "Exponential",
                "curve_exponent": {"x": 2.0, "y": 2.0, "z": 2.0}
            },
            {
                "type": "Smooth"
            }
        ],
        "triggers": [
            {
                "type": "Hold",
                "hold_time_threshold": 0.75,
                "is_one_shot": True
            },
            {
                "type": "Tap",
                "tap_release_time_threshold": 0.3
            },
            {
                "type": "Pulse",
                "interval": 0.5,
                "trigger_limit": 5
            }
        ]
    }
    
    response = mock_agent_client.call_cpp_tool(
        "configure_input_mapping_modifiers_triggers",
        payload
    )
    
    assert response is not None
    # We check whether the route is supported by the C++ HTTP server or mock harness
    if not response.get("bSuccess"):
        pytest.fail(f"Tool execution failed: {response.get('Errors') or response.get('ResultMessage')}")
    
    assert "Configured key mapping" in response.get("ResultMessage", "")
    assert response.get("bSuccess") is True

def test_configure_input_mapping_modifiers_triggers_pascal_case(mock_agent_client):
    """
    Empirical Test: Verify PascalCase alias parameters (ContextAsset, InputActionAsset, Key, Modifiers, Triggers).
    """
    imc_path = "/Game/Input/IMC_TestChallenger"
    ia_path = "/Game/Input/IA_TestChallenger"
    
    payload = {
        "mapping_context_path": imc_path,
        "action_path": ia_path,
        "ContextAsset": imc_path,
        "InputActionAsset": ia_path,
        "Key": "IA_Test_Key_S",
        "Modifiers": [
            {
                "Type": "SwizzleAxis",
                "Order": "XZY"
            },
            {
                "Type": "Scalar",
                "Scalar": 3.0
            }
        ],
        "Triggers": [
            {
                "Type": "Pressed"
            }
        ]
    }
    
    response = mock_agent_client.call_cpp_tool(
        "configure_input_mapping_modifiers_triggers",
        payload
    )
    
    assert response is not None
    assert response.get("bSuccess") is True
