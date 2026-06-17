import os
import sys
import json
import shutil
import tempfile
import asyncio
from pathlib import Path

# Add project root to sys.path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

# Setup temporary directory for isolated testing
TEMP_DIR = tempfile.mkdtemp()
TEMP_WATCH_DIR = Path(TEMP_DIR) / "Source"
TEMP_WATCH_DIR.mkdir(parents=True, exist_ok=True)
TEMP_DB_PATH = Path(TEMP_DIR) / "test_ast_cache_nopending.db"

# Monkeypatch main paths before importing/using them
import ExternalServer.src.main as main
main.WATCH_DIR = TEMP_WATCH_DIR
main.DB_PATH = TEMP_DB_PATH
main.COMPILE_COMMANDS_PATH = Path(TEMP_DIR) / "non_existent_compile_commands.json"

# Re-initialize the database using the monkeypatched DB_PATH
main.init_db()


def clean_temp_dir():
    try:
        shutil.rmtree(TEMP_DIR, ignore_errors=True)
    except Exception:
        pass


def write_test_file(filename: str, content: str) -> str:
    file_path = TEMP_WATCH_DIR / filename
    file_path.write_text(content, encoding="utf-8")
    return str(file_path.resolve())


async def run_query(query: str):
    res_str = await main.query_cpp_ast(query)
    # Parse the json response part
    lines = res_str.split("\n", 1)
    if len(lines) > 1:
        return json.loads(lines[1])
    return res_str


async def test_non_existent_symbol():
    print("\n--- Testing Non-Existent Symbol ---")
    res = await run_query("NonExistentClassXYZ")
    assert isinstance(res, dict)
    assert "error" in res
    assert "not found in AST cache" in res["error"]
    print("Non-existent symbol returned correct fallback result.")


async def test_deeply_nested_classes():
    print("\n--- Testing Deeply Nested Classes ---")
    code = """
    class OuterClass {
    public:
        class InnerClass {
        public:
            class DeepestClass {
            public:
                int nestedField;
                void nestedMethod(int x);
            };
        };
    };
    """
    file_path = write_test_file("Nested.h", code)
    main.parse_cpp_file(file_path)

    # Query the deepest nested class
    res = await run_query("DeepestClass")
    assert isinstance(res, list)
    assert len(res) > 0
    symbol = res[0]
    assert symbol["name"] == "DeepestClass"
    assert symbol["fully_qualified_name"] == "OuterClass::InnerClass::DeepestClass"
    assert symbol["kind"] == "class"
    
    # Verify properties and methods
    assert len(symbol["properties"]) == 1
    assert symbol["properties"][0]["name"] == "nestedField"
    assert symbol["properties"][0]["type"] == "int"
    
    assert len(symbol["methods"]) == 1
    assert symbol["methods"][0]["name"] == "nestedMethod"
    assert len(symbol["methods"][0]["parameters"]) == 1
    assert symbol["methods"][0]["parameters"][0]["name"] == "x"
    assert symbol["methods"][0]["parameters"][0]["type"] == "int"
    print("Nested classes parsed and queried successfully.")


async def test_circular_inheritance():
    print("\n--- Testing Circular Inheritance ---")
    # Circular inheritance is invalid C++, but we test parser robustness
    code = """
    class BChild;
    class AChild : public BChild {
    };
    class BChild : public AChild {
    };
    """
    file_path = write_test_file("Circular.h", code)
    main.parse_cpp_file(file_path)

    res_a = await run_query("AChild")
    assert isinstance(res_a, list)
    assert len(res_a) > 0
    symbol_a = res_a[0]
    assert len(symbol_a["bases"]) > 0
    assert symbol_a["bases"][0]["name"] == "BChild"

    res_b = await run_query("BChild")
    assert isinstance(res_b, list)
    assert len(res_b) > 0
    symbol_b = res_b[0]
    assert len(symbol_b["bases"]) > 0
    assert symbol_b["bases"][0]["name"] == "AChild"
    print("Circular inheritance parsed and queried successfully without loops or crashes.")


async def test_template_classes():
    print("\n--- Testing Template Classes ---")
    code = """
    template <typename T>
    class MyTemplateClass {
    public:
        T data;
        T getData() const { return data; }
    };
    """
    file_path = write_test_file("TemplateClass.h", code)
    main.parse_cpp_file(file_path)

    res = await run_query("MyTemplateClass")
    # It should either parse MyTemplateClass or handle templates gracefully
    assert isinstance(res, list)
    if len(res) > 0:
        symbol = res[0]
        assert "MyTemplateClass" in symbol["name"]
        print(f"Template class queried: {symbol['name']}")
    else:
        print("Template class not indexed (clang sometimes skips uninstantiated templates), but server did not crash.")


async def test_namespaces():
    print("\n--- Testing Namespaces ---")
    code = """
    namespace FirstNS {
        namespace SecondNS {
            class ClassInNamespace {
            public:
                int nsField;
                void nsMethod(double d);
            };
        }
    }
    """
    file_path = write_test_file("Namespaces.h", code)
    main.parse_cpp_file(file_path)

    # Query via class name
    res = await run_query("ClassInNamespace")
    assert isinstance(res, list)
    assert len(res) > 0
    symbol = res[0]
    assert symbol["name"] == "ClassInNamespace"
    assert symbol["fully_qualified_name"] == "FirstNS::SecondNS::ClassInNamespace"
    print("Namespaces parsed and queried successfully.")


async def test_complex_method_signatures():
    print("\n--- Testing Complex Method Signatures ---")
    code = """
    class ComplexClass {
    public:
        virtual const int* processElements(const float* dataPtr, int& count) const;
        static void logMessage(const char* msg);
    };
    """
    file_path = write_test_file("ComplexMethods.h", code)
    main.parse_cpp_file(file_path)

    res = await run_query("ComplexClass")
    assert isinstance(res, list)
    assert len(res) > 0
    symbol = res[0]
    methods = {m["name"]: m for m in symbol["methods"]}
    
    assert "processElements" in methods
    proc = methods["processElements"]
    assert proc["is_virtual"] is True
    assert proc["is_const"] is True
    assert proc["is_static"] is False
    assert "int" in proc["return_type"]
    assert len(proc["parameters"]) == 2
    assert proc["parameters"][0]["name"] == "dataPtr"
    assert "float" in proc["parameters"][0]["type"]
    assert proc["parameters"][1]["name"] == "count"
    assert "int" in proc["parameters"][1]["type"]
    
    assert "logMessage" in methods
    log_m = methods["logMessage"]
    assert log_m["is_static"] is True
    assert log_m["is_virtual"] is False
    print("Complex method signatures parsed and verified successfully.")


async def test_circular_function_calls():
    print("\n--- Testing Circular/Recursive Function Calls ---")
    code = """
    void functionOne();
    void functionTwo();

    void functionOne() {
        functionTwo();
    }

    void functionTwo() {
        functionOne();
    }
    """
    file_path = write_test_file("CircularCalls.h", code)
    main.parse_cpp_file(file_path)

    res = await run_query("functionOne")
    assert isinstance(res, list)
    assert len(res) > 0
    symbol = res[0]
    assert "transitive_callees" in symbol
    # Make sure we don't crash or loop infinitely
    callees = symbol["transitive_callees"]
    print(f"Transitive callees returned successfully (length {len(callees)}).")
    for callee in callees:
        print(f" - Callee: {callee['name']}, Depth: {callee['depth']}")


def run_all_tests():
    print("Starting AST Query Challenger Tests...")
    loop = asyncio.get_event_loop()
    
    tests = [
        test_non_existent_symbol,
        test_deeply_nested_classes,
        test_circular_inheritance,
        test_template_classes,
        test_namespaces,
        test_complex_method_signatures,
        test_circular_function_calls
    ]
    
    failed = 0
    for test in tests:
        try:
            loop.run_until_complete(test())
            print(f"SUCCESS: {test.__name__}")
        except Exception as e:
            import traceback
            print(f"FAILURE: {test.__name__} - {e}")
            traceback.print_exc()
            failed += 1
            
    clean_temp_dir()
    print(f"\nTest run finished. Failures: {failed}/{len(tests)}")
    if failed > 0:
        sys.exit(1)


if __name__ == "__main__":
    run_all_tests()
