import sys
from pathlib import Path

# Add src directory to path
src_dir = Path(__file__).parent.parent / "UnrealEngine" / "ExternalServer" / "src"
sys.path.insert(0, str(src_dir))

from t3d_layout import format_layout

def test_t3d_layout_basic():
    t3d_input = """Begin Object Class=/Script/BlueprintGraph.K2Node_Event Name="K2Node_Event_0"
   NodePosX=0
   NodePosY=0
   CustomProperties Pin (PinId=P1,PinName="then",Direction=EGPD_Output,PinType.PinCategory="exec",LinkedTo=(K2Node_CallFunction_0 P2))
End Object
Begin Object Class=/Script/BlueprintGraph.K2Node_CallFunction Name="K2Node_CallFunction_0"
   NodePosX=0
   NodePosY=0
   CustomProperties Pin (PinId=P2,PinName="execute",PinType.PinCategory="exec",LinkedTo=(K2Node_Event_0 P1))
   CustomProperties Pin (PinId=P3,PinName="then",Direction=EGPD_Output,PinType.PinCategory="exec")
End Object"""

    output = format_layout(t3d_input)
    assert "NodePosX=350" in output
    assert "NodePosY=0" in output

def test_t3d_layout_complex():
    # Event -> Branch
    # Branch -> True: PrintString1
    #        -> False: PrintString2
    # PrintString1 -> Input text from variable getter 1 and variable getter 2
    t3d_input = """Begin Object Class=/Script/BlueprintGraph.K2Node_Event Name="K2Node_Event_0"
   NodePosX=0
   NodePosY=0
   CustomProperties Pin (PinId=P1,PinName="then",Direction=EGPD_Output,PinType.PinCategory="exec",LinkedTo=(K2Node_IfThenElse_0 P2))
End Object
Begin Object Class=/Script/BlueprintGraph.K2Node_IfThenElse Name="K2Node_IfThenElse_0"
   NodePosX=0
   NodePosY=0
   CustomProperties Pin (PinId=P2,PinName="execute",PinType.PinCategory="exec",LinkedTo=(K2Node_Event_0 P1))
   CustomProperties Pin (PinId=P3,PinName="then",PinType.PinCategory="bool")
   CustomProperties Pin (PinId=P4,PinName="true",Direction=EGPD_Output,PinType.PinCategory="exec",LinkedTo=(K2Node_CallFunction_True P5))
   CustomProperties Pin (PinId=P6,PinName="false",Direction=EGPD_Output,PinType.PinCategory="exec",LinkedTo=(K2Node_CallFunction_False P7))
End Object
Begin Object Class=/Script/BlueprintGraph.K2Node_CallFunction Name="K2Node_CallFunction_True"
   NodePosX=0
   NodePosY=0
   CustomProperties Pin (PinId=P5,PinName="execute",PinType.PinCategory="exec",LinkedTo=(K2Node_IfThenElse_0 P4))
   CustomProperties Pin (PinId=P8,PinName="InString",PinType.PinCategory="string",LinkedTo=(K2Node_VariableGet_0 P9))
   CustomProperties Pin (PinId=P10,PinName="InString2",PinType.PinCategory="string",LinkedTo=(K2Node_VariableGet_1 P11))
End Object
Begin Object Class=/Script/BlueprintGraph.K2Node_CallFunction Name="K2Node_CallFunction_False"
   NodePosX=0
   NodePosY=0
   CustomProperties Pin (PinId=P7,PinName="execute",PinType.PinCategory="exec",LinkedTo=(K2Node_IfThenElse_0 P6))
End Object
Begin Object Class=/Script/BlueprintGraph.K2Node_VariableGet Name="K2Node_VariableGet_0"
   NodePosX=0
   NodePosY=0
   CustomProperties Pin (PinId=P9,PinName="MyVar",Direction=EGPD_Output,PinType.PinCategory="string",LinkedTo=(K2Node_CallFunction_True P8))
End Object
Begin Object Class=/Script/BlueprintGraph.K2Node_VariableGet Name="K2Node_VariableGet_1"
   NodePosX=0
   NodePosY=0
   CustomProperties Pin (PinId=P11,PinName="MyVar2",Direction=EGPD_Output,PinType.PinCategory="string",LinkedTo=(K2Node_CallFunction_True P10))
End Object"""

    output = format_layout(t3d_input)
    print("Formatted output complex:")
    print(output)
    
    assert 'Name="K2Node_Event_0"\n   NodePosX=0\n   NodePosY=0' in output
    assert 'Name="K2Node_IfThenElse_0"\n   NodePosX=350\n   NodePosY=0' in output
    assert 'Name="K2Node_CallFunction_True"\n   NodePosX=700\n   NodePosY=0' in output
    assert 'Name="K2Node_CallFunction_False"\n   NodePosX=700\n   NodePosY=200' in output
    # First dep sits at same Y (0)
    assert 'Name="K2Node_VariableGet_0"\n   NodePosX=450\n   NodePosY=0' in output
    # Second dep sits at offset Y (150)
    assert 'Name="K2Node_VariableGet_1"\n   NodePosX=450\n   NodePosY=150' in output

if __name__ == "__main__":
    test_t3d_layout_basic()
    test_t3d_layout_complex()
    print("All tests passed!")
