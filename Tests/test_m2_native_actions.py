import pytest
import json

def test_find_unreferenced_assets_schema_validation(mock_agent_client):
    """
    Test calling find_unreferenced_assets with missing required parameter.
    """
    res = mock_agent_client.call_cpp_tool(
        "find_unreferenced_assets",
        {}
    )
    assert res is not None
    assert res.get("bSuccess") is False, f"Expected bSuccess=False, got {res}"
    assert any("folder_path" in str(err).lower() for err in res.get("Errors", []))

def test_inspect_uobject_properties_schema_validation(mock_agent_client):
    """
    Test calling inspect_uobject_properties with missing required parameter.
    """
    res = mock_agent_client.call_cpp_tool(
        "inspect_uobject_properties",
        {}
    )
    assert res is not None
    assert res.get("bSuccess") is False, f"Expected bSuccess=False, got {res}"
    assert any("object_path" in str(err).lower() for err in res.get("Errors", []))

def test_consolidate_asset_references_schema_validation(mock_agent_client):
    """
    Test calling consolidate_asset_references with missing required parameters.
    """
    res = mock_agent_client.call_cpp_tool(
        "consolidate_asset_references",
        {}
    )
    assert res is not None
    assert res.get("bSuccess") is False, f"Expected bSuccess=False, got {res}"
    assert any("source_asset_path" in str(err).lower() or "target_asset_path" in str(err).lower() for err in res.get("Errors", []))
