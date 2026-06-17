import os
import sys
import json
import shutil
import tempfile
import asyncio
import pytest
from pathlib import Path

# Add project root to sys.path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

# Setup temporary directory for isolated testing
TEMP_DIR = tempfile.mkdtemp()
TEMP_WATCH_DIR = Path(TEMP_DIR) / "Source"
TEMP_WATCH_DIR.mkdir(parents=True, exist_ok=True)
TEMP_DB_PATH = Path(TEMP_DIR) / "test_ast_cache_outofline.db"

# Monkeypatch main paths before importing/using them
import UnrealEngine.ExternalServer.src.main as main
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


@pytest.mark.asyncio
async def test_overloaded_functions():
    print("\n--- Testing Overloaded Functions ---")
    code = """
    void overloadedFunc(int x);
    void overloadedFunc(double y);

    void callerFunc() {
        overloadedFunc(42);
        overloadedFunc(3.14);
    }
    """
    file_path = write_test_file("Overloads.h", code)
    main.parse_cpp_file(file_path)

    # Query the caller function to check transitive callees or function calls
    res = await run_query("callerFunc")
    assert isinstance(res, list)
    assert len(res) > 0
    symbol = res[0]
    
    assert "transitive_callees" in symbol
    callees = symbol["transitive_callees"]
    # We should have two callees: overloadedFunc(int) and overloadedFunc(double)
    # Check if we successfully matched them
    callee_names = [c["name"] for c in callees]
    print(f"Callee names: {callee_names}")
    assert "overloadedFunc" in callee_names


@pytest.mark.asyncio
async def test_outofline_methods():

    print("\n--- Testing Out-of-Line Method Definitions ---")
    header_code = """
    class MySimpleClass {
    public:
        void myMethod(int a);
    };
    """
    source_code = """
    #include "MySimpleClass.h"
    void MySimpleClass::myMethod(int a) {
        // definition
    }
    """
    h_path = write_test_file("MySimpleClass.h", header_code)
    cpp_path = write_test_file("MySimpleClass.cpp", source_code)
    
    # Parse header then source
    main.parse_cpp_file(h_path)
    main.parse_cpp_file(cpp_path)

    # Query myMethod
    res = await run_query("MySimpleClass::myMethod")
    assert isinstance(res, list)
    # We expect to find both the declaration and definition, or at least the definition correctly associated
    print(f"Query MySimpleClass::myMethod results count: {len(res)}")
    for sym in res:
        print(f" - FQN: {sym['fully_qualified_name']}, File: {sym['file_path']}, Line: {sym['line_start']}")
    
    assert len(res) >= 1
    # Check that at least one resolved fully qualified name is MySimpleClass::myMethod
    fqns = [sym["fully_qualified_name"] for sym in res]
    assert "MySimpleClass::myMethod" in fqns


@pytest.mark.asyncio
async def test_namespaced_outofline_methods():
    print("\n--- Testing Namespaced Out-of-Line Method Definitions ---")
    header_code = """
    namespace MyNS {
        class MyNamespacedClass {
        public:
            void myNsMethod(int x);
        };
    }
    """
    source_code = """
    #include "MyNamespacedClass.h"
    void MyNS::MyNamespacedClass::myNsMethod(int x) {
        // definition
    }
    """
    h_path = write_test_file("MyNamespacedClass.h", header_code)
    cpp_path = write_test_file("MyNamespacedClass.cpp", source_code)
    
    main.parse_cpp_file(h_path)
    main.parse_cpp_file(cpp_path)

    res = await run_query("MyNamespacedClass::myNsMethod")
    assert isinstance(res, list)
    print(f"Query MyNamespacedClass::myNsMethod results count: {len(res)}")
    for sym in res:
        print(f" - FQN: {sym['fully_qualified_name']}, File: {sym['file_path']}, Line: {sym['line_start']}")
    
    assert len(res) >= 1
    fqns = [sym["fully_qualified_name"] for sym in res]
    assert "MyNS::MyNamespacedClass::myNsMethod" in fqns


def run_all_tests():
    print("Starting AST Query Out-of-Line & Overload Tests...")
    loop = asyncio.get_event_loop()
    
    tests = [
        test_overloaded_functions,
        test_outofline_methods,
        test_namespaced_outofline_methods
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
