import pytest
import json

def test_configure_input_mapping_empty_modifiers(mock_agent_client):
    """
    Adversarial Test: Verify empty or omitted modifiers array.
    Should clear existing modifiers and apply 0 modifiers successfully.
    """
    imc_path = "/Game/Input/IMC_TestChallenger"
    ia_path = "/Game/Input/IA_TestChallenger"
    
    payload = {
        "mapping_context_path": imc_path,
        "action_path": ia_path,
        "key": "W",
        "modifiers": [],
        "triggers": [{"type": "Pressed"}]
    }
    
    response = mock_agent_client.call_cpp_tool(
        "configure_input_mapping_modifiers_triggers",
        payload
    )
    
    assert response is not None
    assert response.get("bSuccess") is True
    assert "0 modifiers" in response.get("ResultMessage", "")
    assert "1 triggers" in response.get("ResultMessage", "")

def test_configure_input_mapping_empty_triggers_guardrail(mock_agent_client):
    """
    Adversarial Test: Verify empty triggers array triggers default UInputTriggerPressed guardrail.
    Should automatically attach UInputTriggerPressed and report 1 trigger applied.
    """
    imc_path = "/Game/Input/IMC_TestChallenger"
    ia_path = "/Game/Input/IA_TestChallenger"
    
    payload = {
        "mapping_context_path": imc_path,
        "action_path": ia_path,
        "key": "A",
        "modifiers": [{"type": "Negate"}],
        "triggers": []
    }
    
    response = mock_agent_client.call_cpp_tool(
        "configure_input_mapping_modifiers_triggers",
        payload
    )
    
    assert response is not None
    assert response.get("bSuccess") is True
    assert "1 modifiers" in response.get("ResultMessage", "")
    assert "1 triggers" in response.get("ResultMessage", "")

def test_configure_input_mapping_triggers_default_keyword(mock_agent_client):
    """
    Adversarial Test: Verify 'default' keyword in triggers array triggers guardrail.
    """
    imc_path = "/Game/Input/IMC_TestChallenger"
    ia_path = "/Game/Input/IA_TestChallenger"
    
    payload = {
        "mapping_context_path": imc_path,
        "action_path": ia_path,
        "key": "S",
        "triggers": ["default"]
    }
    
    response = mock_agent_client.call_cpp_tool(
        "configure_input_mapping_modifiers_triggers",
        payload
    )
    
    assert response is not None
    assert response.get("bSuccess") is True
    assert "1 triggers" in response.get("ResultMessage", "")

def test_configure_input_mapping_non_existent_imc(mock_agent_client):
    """
    Adversarial Test: Non-existent IMC asset path should fail gracefully with descriptive error.
    """
    payload = {
        "mapping_context_path": "/Game/Input/IMC_NonExistent_999999",
        "action_path": "/Game/Input/IA_TestChallenger",
        "key": "D"
    }
    
    response = mock_agent_client.call_cpp_tool(
        "configure_input_mapping_modifiers_triggers",
        payload
    )
    
    assert response is not None
    assert response.get("bSuccess") is False
    errors = response.get("Errors", [])
    assert any("Could not load Input Mapping Context" in err for err in errors)

def test_configure_input_mapping_non_existent_ia(mock_agent_client):
    """
    Adversarial Test: Non-existent IA asset path should fail gracefully with descriptive error.
    """
    payload = {
        "mapping_context_path": "/Game/Input/IMC_TestChallenger",
        "action_path": "/Game/Input/IA_NonExistent_999999",
        "key": "SpaceBar"
    }
    
    response = mock_agent_client.call_cpp_tool(
        "configure_input_mapping_modifiers_triggers",
        payload
    )
    
    assert response is not None
    assert response.get("bSuccess") is False
    errors = response.get("Errors", [])
    assert any("Could not load Input Action" in err for err in errors)

def test_configure_input_mapping_unknown_modifier_and_trigger(mock_agent_client):
    """
    Adversarial Test: Unknown modifier/trigger types should be skipped gracefully.
    Triggers will fall back to default Pressed trigger guardrail.
    """
    payload = {
        "mapping_context_path": "/Game/Input/IMC_TestChallenger",
        "action_path": "/Game/Input/IA_TestChallenger",
        "key": "SpaceBar",
        "modifiers": [{"type": "UnknownModifier_X"}],
        "triggers": [{"type": "UnknownTrigger_Y"}]
    }
    
    response = mock_agent_client.call_cpp_tool(
        "configure_input_mapping_modifiers_triggers",
        payload
    )
    
    assert response is not None
    assert response.get("bSuccess") is True
    assert "0 modifiers" in response.get("ResultMessage", "")
    assert "1 triggers" in response.get("ResultMessage", "")
