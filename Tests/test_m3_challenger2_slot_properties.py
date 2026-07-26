import pytest
import json

def test_set_widget_slot_properties_snake_case_and_pascal_case_aliasing(mock_agent_client):
    """
    Challenge 1: Parameter Aliasing.
    Verify both snake_case and PascalCase parameter aliases for set_widget_slot_properties.
    """
    bp_path = "/Game/UI/W_ChallengerTest"
    
    # 1. Create a dummy widget blueprint first if needed or test tool structure
    res_create = mock_agent_client.call_cpp_tool(
        "create_widget_blueprint",
        {"asset_path": bp_path, "root_widget_class": "CanvasPanel"}
    )
    assert res_create is not None
    
    # Add a Button to CanvasPanel
    res_add_btn = mock_agent_client.call_cpp_tool(
        "add_widget",
        {"asset_path": bp_path, "widget_class": "Button", "widget_name": "TestBtn", "parent_widget": "CanvasPanel_0"}
    )
    assert res_add_btn is not None
    
    # Test snake_case payload for CanvasPanelSlot
    payload_snake = {
        "asset_path": bp_path,
        "widget_name": "TestBtn",
        "slot_properties": {
            "anchors": {"min_x": 0.1, "min_y": 0.1, "max_x": 0.9, "max_y": 0.9},
            "offsets": {"left": 10.0, "top": 20.0, "right": 30.0, "bottom": 40.0},
            "alignment": {"x": 0.5, "y": 0.5},
            "auto_size": True,
            "z_order": 10
        }
    }
    res_snake = mock_agent_client.call_cpp_tool("set_widget_slot_properties", payload_snake)
    assert res_snake is not None
    assert res_snake.get("bSuccess") is True, f"Snake case set_widget_slot_properties failed: {res_snake}"
    
    # Test PascalCase payload for CanvasPanelSlot
    payload_pascal = {
        "AssetPath": bp_path,
        "WidgetName": "TestBtn",
        "SlotProperties": {
            "Anchors": {"MinX": 0.0, "MinY": 0.0, "MaxX": 1.0, "MaxY": 1.0},
            "Offsets": {"Left": 0.0, "Top": 0.0, "Right": 100.0, "Bottom": 50.0},
            "Alignment": {"X": 0.0, "Y": 0.0},
            "AutoSize": False,
            "ZOrder": 5
        }
    }
    res_pascal = mock_agent_client.call_cpp_tool("set_widget_slot_properties", payload_pascal)
    assert res_pascal is not None
    assert res_pascal.get("bSuccess") is True, f"Pascal case set_widget_slot_properties failed: {res_pascal}"


def test_set_widget_slot_properties_nested_vs_string_parsing(mock_agent_client):
    """
    Challenge 2: Nested JSON vs String representations.
    Verify both nested JSON objects and string representations are correctly parsed.
    """
    bp_path = "/Game/UI/W_ChallengerTest"
    
    # Test nested JSON object representations
    payload_nested = {
        "asset_path": bp_path,
        "widget_name": "TestBtn",
        "anchors": {"min_x": 0.2, "min_y": 0.2, "max_x": 0.8, "max_y": 0.8},
        "offsets": {"left": 5, "top": 5, "right": 5, "bottom": 5},
        "alignment": {"x": 0.5, "y": 0.5}
    }
    res_nested = mock_agent_client.call_cpp_tool("set_widget_slot_properties", payload_nested)
    assert res_nested is not None
    assert res_nested.get("bSuccess") is True, f"Nested object parsing failed: {res_nested}"
    
    # Test string representations
    payload_string = {
        "asset_path": bp_path,
        "widget_name": "TestBtn",
        "anchors": "0.0, 0.0, 1.0, 1.0",
        "offsets": "10, 10, 20, 20",
        "alignment": "0.5, 0.5"
    }
    res_string = mock_agent_client.call_cpp_tool("set_widget_slot_properties", payload_string)
    assert res_string is not None
    assert res_string.get("bSuccess") is True, f"String representation parsing failed: {res_string}"


def test_set_widget_slot_properties_all_12_slot_types(mock_agent_client):
    """
    Challenge 3: Coverage across all 12 slot types.
    Verify property setting across CanvasPanelSlot, VerticalBoxSlot, HorizontalBoxSlot, OverlaySlot,
    GridSlot, UniformGridSlot, ScrollBoxSlot, WrapBoxSlot, WidgetSwitcherSlot, ScaleBoxSlot,
    BorderSlot, SizeBoxSlot.
    """
    bp_path = "/Game/UI/W_ChallengerAllSlots"
    
    # Create Blueprint with CanvasPanel root
    mock_agent_client.call_cpp_tool("create_widget_blueprint", {"asset_path": bp_path, "root_widget_class": "CanvasPanel"})
    
    # Slot 1: CanvasPanelSlot (Child directly on CanvasPanel)
    mock_agent_client.call_cpp_tool("add_widget", {"asset_path": bp_path, "widget_class": "TextBlock", "widget_name": "CanvasChild", "parent_widget": "CanvasPanel_0"})
    res_canvas = mock_agent_client.call_cpp_tool("set_widget_slot_properties", {
        "asset_path": bp_path, "widget_name": "CanvasChild",
        "anchors": {"min_x": 0, "min_y": 0, "max_x": 1, "max_y": 1},
        "offsets": {"left": 0, "top": 0, "right": 0, "bottom": 0},
        "alignment": {"x": 0.5, "y": 0.5},
        "auto_size": True, "z_order": 2
    })
    assert res_canvas.get("bSuccess") is True, f"CanvasPanelSlot failed: {res_canvas}"
    
    # Helper to test slot setting on child inside container panel
    def check_panel_slot(panel_class, panel_name, child_name, slot_params):
        mock_agent_client.call_cpp_tool("add_widget", {"asset_path": bp_path, "widget_class": panel_class, "widget_name": panel_name, "parent_widget": "CanvasPanel_0"})
        mock_agent_client.call_cpp_tool("add_widget", {"asset_path": bp_path, "widget_class": "TextBlock", "widget_name": child_name, "parent_widget": panel_name})
        payload = {"asset_path": bp_path, "widget_name": child_name, "slot_properties": slot_params}
        res = mock_agent_client.call_cpp_tool("set_widget_slot_properties", payload)
        assert res.get("bSuccess") is True, f"Slot test failed for {panel_class} / {child_name}: {res}"

    # Slot 2: VerticalBoxSlot
    check_panel_slot("VerticalBox", "VBox_0", "VBChild", {"padding": {"left": 5, "top": 5, "right": 5, "bottom": 5}, "size_rule": "Fill", "h_align": "Center", "v_align": "Top"})

    # Slot 3: HorizontalBoxSlot
    check_panel_slot("HorizontalBox", "HBox_0", "HBChild", {"padding": "10, 10", "size": "Auto", "h_align": "Left", "v_align": "Fill"})

    # Slot 4: OverlaySlot
    check_panel_slot("Overlay", "Overlay_0", "OverlayChild", {"padding": 15, "h_align": "Right", "v_align": "Bottom"})

    # Slot 5: GridSlot
    check_panel_slot("GridPanel", "Grid_0", "GridChild", {"row": 1, "column": 2, "row_span": 2, "column_span": 3, "layer": 1, "nudge": {"x": 5, "y": 5}, "padding": 4, "h_align": "Center", "v_align": "Center"})

    # Slot 6: UniformGridSlot
    check_panel_slot("UniformGridPanel", "UGrid_0", "UGridChild", {"row": 0, "column": 1, "h_align": "Fill", "v_align": "Fill"})

    # Slot 7: ScrollBoxSlot
    check_panel_slot("ScrollBox", "Scroll_0", "ScrollChild", {"padding": {"uniform": 8}, "h_align": "Left", "v_align": "Top"})

    # Slot 8: WrapBoxSlot
    check_panel_slot("WrapBox", "Wrap_0", "WrapChild", {"padding": 2, "fill_empty_space": True, "fill_span_when_less_than": 200.0, "h_align": "Center", "v_align": "Center"})

    # Slot 9: WidgetSwitcherSlot
    check_panel_slot("WidgetSwitcher", "Switcher_0", "SwitcherChild", {"padding": 0, "h_align": "Fill", "v_align": "Fill"})

    # Slot 10: ScaleBoxSlot
    check_panel_slot("ScaleBox", "Scale_0", "ScaleChild", {"padding": 5, "h_align": "Center", "v_align": "Center"})

    # Slot 11: BorderSlot
    check_panel_slot("Border", "Border_0", "BorderChild", {"padding": 12, "h_align": "Fill", "v_align": "Fill"})

    # Slot 12: SizeBoxSlot
    check_panel_slot("SizeBox", "Size_0", "SizeChild", {"padding": 3, "h_align": "Center", "v_align": "Top"})
