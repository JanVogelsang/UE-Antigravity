import json
import urllib.request
import sys

URL = "http://127.0.0.1:18777/api/execute_tool"

def call_tool(tool_name, args):
    payload = json.dumps({"name": tool_name, "arguments": args}).encode('utf-8')
    req = urllib.request.Request(URL, data=payload, headers={'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=30.0) as resp:
            return json.loads(resp.read().decode('utf-8'))
    except Exception as e:
        return {"bSuccess": False, "Errors": [str(e)]}

def main():
    print("=== Starting Niagara set_niagara_parameter Verification ===")
    
    # 1. Create/Ensure Niagara System
    system_path = "/Game/Effects/NS_TestParameterSystem"
    print(f"\n[1] Ensuring Niagara System at {system_path}...")
    res = call_tool("create_niagara_system", {"asset_path": system_path})
    print(f"    Create System Result: bSuccess={res.get('bSuccess')}, ResultMessage={res.get('ResultMessage')}, Errors={res.get('Errors')}")
    
    # 2. Float Parameter (snake_case)
    print("\n[2] Testing Float Parameter (snake_case)...")
    res = call_tool("set_niagara_parameter", {
        "system_path": system_path,
        "parameter_scope": "User",
        "parameter_name": "SpawnRate",
        "data_type": "Float",
        "value": 150.5
    })
    print(f"    Result: bSuccess={res.get('bSuccess')}, Msg={res.get('ResultMessage')}, Errors={res.get('Errors')}")
    assert res.get("bSuccess") is True

    # 3. Float Parameter (PascalCase)
    print("\n[3] Testing Float Parameter (PascalCase)...")
    res = call_tool("set_niagara_parameter", {
        "SystemAsset": system_path,
        "ParameterScope": "User",
        "ParameterName": "SpawnRate",
        "DataType": "Float",
        "Value": 250.0
    })
    print(f"    Result: bSuccess={res.get('bSuccess')}, Msg={res.get('ResultMessage')}, Errors={res.get('Errors')}")
    assert res.get("bSuccess") is True

    # 4. Vector2 Parameter (Object & Array)
    print("\n[4] Testing Vector2 Parameter...")
    res = call_tool("set_niagara_parameter", {
        "system_path": system_path,
        "parameter_name": "Scale2D",
        "data_type": "Vector2",
        "value": {"x": 2.5, "y": 5.0}
    })
    print(f"    Object Result: bSuccess={res.get('bSuccess')}, Msg={res.get('ResultMessage')}")
    assert res.get("bSuccess") is True

    res = call_tool("set_niagara_parameter", {
        "system_path": system_path,
        "parameter_name": "Scale2D",
        "data_type": "Vector2",
        "value": [3.0, 6.0]
    })
    print(f"    Array Result: bSuccess={res.get('bSuccess')}, Msg={res.get('ResultMessage')}")
    assert res.get("bSuccess") is True

    # 5. Vector3 Parameter (Object & Array)
    print("\n[5] Testing Vector3 Parameter...")
    res = call_tool("set_niagara_parameter", {
        "system_path": system_path,
        "parameter_name": "Velocity3D",
        "data_type": "Vector3",
        "value": {"x": 100.0, "y": 0.0, "z": -980.0}
    })
    print(f"    Object Result: bSuccess={res.get('bSuccess')}, Msg={res.get('ResultMessage')}")
    assert res.get("bSuccess") is True

    res = call_tool("set_niagara_parameter", {
        "SystemAsset": system_path,
        "ParameterName": "Velocity3D",
        "DataType": "Vector3",
        "Value": [0.0, 500.0, 200.0]
    })
    print(f"    Array Result: bSuccess={res.get('bSuccess')}, Msg={res.get('ResultMessage')}")
    assert res.get("bSuccess") is True

    # 6. LinearColor Parameter (Object & Array)
    print("\n[6] Testing LinearColor Parameter...")
    res = call_tool("set_niagara_parameter", {
        "system_path": system_path,
        "parameter_name": "PrimaryColor",
        "data_type": "LinearColor",
        "value": {"r": 1.0, "g": 0.2, "b": 0.1, "a": 1.0}
    })
    print(f"    Object Result: bSuccess={res.get('bSuccess')}, Msg={res.get('ResultMessage')}")
    assert res.get("bSuccess") is True

    res = call_tool("set_niagara_parameter", {
        "SystemAsset": system_path,
        "ParameterName": "PrimaryColor",
        "DataType": "LinearColor",
        "Value": [0.0, 1.0, 0.5, 0.8]
    })
    print(f"    Array Result: bSuccess={res.get('bSuccess')}, Msg={res.get('ResultMessage')}")
    assert res.get("bSuccess") is True

    # 7. Bool and Int32 Parameters
    print("\n[7] Testing Bool and Int32 Parameters...")
    res_bool = call_tool("set_niagara_parameter", {
        "system_path": system_path,
        "parameter_name": "bEnabled",
        "data_type": "Bool",
        "value": True
    })
    print(f"    Bool Result: bSuccess={res_bool.get('bSuccess')}, Msg={res_bool.get('ResultMessage')}")
    assert res_bool.get("bSuccess") is True

    res_int = call_tool("set_niagara_parameter", {
        "system_path": system_path,
        "parameter_name": "ParticleCount",
        "data_type": "Int32",
        "value": 500
    })
    print(f"    Int32 Result: bSuccess={res_int.get('bSuccess')}, Msg={res_int.get('ResultMessage')}")
    assert res_int.get("bSuccess") is True

    # 8. CurveFloat Parameter
    print("\n[8] Testing CurveFloat Parameter...")
    res_cf1 = call_tool("set_niagara_parameter", {
        "system_path": system_path,
        "parameter_name": "FloatCurveOverLife",
        "data_type": "CurveFloat",
        "curve_keys": [
            {"time": 0.0, "value": 0.0},
            {"time": 0.5, "value": 1.5},
            {"time": 1.0, "value": 0.0}
        ]
    })
    print(f"    snake_case CurveFloat Result: bSuccess={res_cf1.get('bSuccess')}, Msg={res_cf1.get('ResultMessage')}")
    assert res_cf1.get("bSuccess") is True

    res_cf2 = call_tool("set_niagara_parameter", {
        "SystemAsset": system_path,
        "ParameterName": "FloatCurveOverLife",
        "DataType": "CurveFloat",
        "CurveKeys": [
            {"Time": 0.0, "Value": 1.0},
            {"Time": 1.0, "Value": 0.1}
        ]
    })
    print(f"    PascalCase CurveFloat Result: bSuccess={res_cf2.get('bSuccess')}, Msg={res_cf2.get('ResultMessage')}")
    assert res_cf2.get("bSuccess") is True

    # 9. CurveLinearColor Parameter
    print("\n[9] Testing CurveLinearColor Parameter...")
    res_cc1 = call_tool("set_niagara_parameter", {
        "system_path": system_path,
        "parameter_name": "ColorCurveOverLife",
        "data_type": "CurveLinearColor",
        "curve_keys": [
            {"time": 0.0, "r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0},
            {"time": 0.5, "r": 0.0, "g": 1.0, "b": 0.0, "a": 1.0},
            {"time": 1.0, "r": 0.0, "g": 0.0, "b": 1.0, "a": 1.0}
        ]
    })
    print(f"    RGBA Keyframes Result: bSuccess={res_cc1.get('bSuccess')}, Msg={res_cc1.get('ResultMessage')}")
    assert res_cc1.get("bSuccess") is True

    res_cc2 = call_tool("set_niagara_parameter", {
        "SystemAsset": system_path,
        "ParameterName": "ColorCurveOverLife",
        "DataType": "CurveLinearColor",
        "CurveKeys": [
            {"Time": 0.0, "Value": [1.0, 1.0, 0.0, 1.0]},
            {"Time": 1.0, "Value": [0.0, 0.0, 0.0, 0.0]}
        ]
    })
    print(f"    Array Keyframes Result: bSuccess={res_cc2.get('bSuccess')}, Msg={res_cc2.get('ResultMessage')}")
    assert res_cc2.get("bSuccess") is True

    # 10. Error cases
    print("\n[10] Testing Error Cases...")
    res_err1 = call_tool("set_niagara_parameter", {
        "system_path": "/Game/Effects/NS_NonExistentSystem_999",
        "parameter_name": "TestParam",
        "data_type": "Float",
        "value": 1.0
    })
    print(f"    Non-existent System Result: bSuccess={res_err1.get('bSuccess')}, Errors={res_err1.get('Errors')}")
    assert res_err1.get("bSuccess") is False

    res_err2 = call_tool("set_niagara_parameter", {
        "system_path": system_path,
        "parameter_name": "TestParam",
        "data_type": "InvalidDataType",
        "value": 1.0
    })
    print(f"    Invalid DataType Result: bSuccess={res_err2.get('bSuccess')}, Errors={res_err2.get('Errors')}")
    assert res_err2.get("bSuccess") is False

    print("\n=== ALL EMPIRICAL TESTS PASSED SUCCESSFULLY ===")

if __name__ == "__main__":
    main()
