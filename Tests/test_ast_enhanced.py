import os
import sys
import json
import shutil
import tempfile
import asyncio
import time
import pytest
from pathlib import Path

# Add project root to sys.path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

# Setup temporary directory for isolated testing
TEMP_DIR = tempfile.mkdtemp()
TEMP_WATCH_DIR = Path(TEMP_DIR) / "Source"
TEMP_WATCH_DIR.mkdir(parents=True, exist_ok=True)
TEMP_DB_PATH = Path(TEMP_DIR) / "test_ast_enhanced_cache.db"

# Import main and monkeypatch paths before running tests
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


async def parse_json_tool_response(res_str: str) -> dict:
    lines = res_str.split("\n", 1)
    if len(lines) > 1:
        return json.loads(lines[1])
    return json.loads(res_str)


@pytest.mark.asyncio
async def test_realtime_header_watch_updates():
    print("\n--- Testing Real-Time Header File Watch Updates ---")
    header_code_v1 = """
    class FWatchTestClass {
    public:
        void InitialMethod();
    };
    """
    header_path = write_test_file("WatchHeaderTest.h", header_code_v1)
    main.parse_cpp_file(header_path, force=True)

    res_v1 = await parse_json_tool_response(await main.query_cpp_ast("FWatchTestClass"))
    assert isinstance(res_v1, list)
    assert len(res_v1) > 0
    methods_v1 = [m["name"] for m in res_v1[0]["methods"]]
    assert "InitialMethod" in methods_v1
    assert "UpdatedMethod" not in methods_v1

    # Modify header file to simulate real-time update
    header_code_v2 = """
    class FWatchTestClass {
    public:
        void InitialMethod();
        void UpdatedMethod(int param);
    };
    """
    time.sleep(0.1)
    write_test_file("WatchHeaderTest.h", header_code_v2)
    main.parse_cpp_file(header_path, force=True)

    res_v2 = await parse_json_tool_response(await main.query_cpp_ast("FWatchTestClass"))
    assert isinstance(res_v2, list)
    assert len(res_v2) > 0
    methods_v2 = [m["name"] for m in res_v2[0]["methods"]]
    assert "InitialMethod" in methods_v2
    assert "UpdatedMethod" in methods_v2
    print("Real-time header update successfully re-indexed and refreshed AST cache.")


@pytest.mark.asyncio
async def test_fallback_file_watcher():
    print("\n--- Testing Fallback Background File Watcher Thread ---")
    watcher = main.FallbackFileWatcher(TEMP_WATCH_DIR, interval=0.5)
    watcher.start()
    try:
        header_code = """
        class FFallbackTestClass {
        public:
            int FallbackField;
        };
        """
        write_test_file("FallbackTest.h", header_code)
        
        # Wait for fallback watcher polling cycle
        time.sleep(1.5)
        
        res = await parse_json_tool_response(await main.query_cpp_ast("FFallbackTestClass"))
        assert isinstance(res, list)
        assert len(res) > 0
        assert res[0]["name"] == "FFallbackTestClass"
        print("Fallback background watcher successfully detected file and updated SQLite AST DB.")
    finally:
        watcher.stop()


@pytest.mark.asyncio
async def test_macro_expansion_inspection():
    print("\n--- Testing Macro Expansion Inspection ---")
    macro_code = """
    #define MY_CALC_MACRO(X, Y) ((X) * (Y) + 10)

    UCLASS(Blueprintable, Category="TestClass")
    class UTestMacroActor {
        GENERATED_BODY()

        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 HealthScore;

        UFUNCTION(BlueprintCallable)
        void ExecuteAction();
    };
    """
    macro_file = write_test_file("MacroTest.h", macro_code)
    main.parse_cpp_file(macro_file, force=True)

    # 1. Inspect custom macro definition
    res_def = await parse_json_tool_response(await main.inspect_macro_expansion("MY_CALC_MACRO"))
    assert res_def["macro_name"] == "MY_CALC_MACRO"
    assert res_def["total_definitions"] >= 1
    assert "X" in res_def["definitions"][0]["parameters"]
    assert "Y" in res_def["definitions"][0]["parameters"]

    # 2. Inspect UE UCLASS macro expansion
    res_uclass = await parse_json_tool_response(await main.query_macro_expansion("UCLASS"))
    assert res_uclass["macro_name"] == "UCLASS"
    assert res_uclass["total_expansions"] >= 1
    assert any("Blueprintable" in item["parameters"] for item in res_uclass["expansions"])

    # 3. Inspect UPROPERTY macro expansion
    res_uprop = await parse_json_tool_response(await main.inspect_macro_expansion("UPROPERTY"))
    assert res_uprop["total_expansions"] >= 1
    assert any("EditAnywhere" in item["parameters"] for item in res_uprop["expansions"])

    print("Macro expansion inspection returned complete definition, parameters, and expansion metadata.")


@pytest.mark.asyncio
async def test_call_graph_visualization():
    print("\n--- Testing Multi-File Call Graph Visualization ---")
    file_a_code = """
    void HelperFunctionB() {
        // leaf function
    }

    void ProcessDataA() {
        HelperFunctionB();
    }
    """
    file_b_code = """
    void ProcessDataA();

    void MainEntryRunner() {
        ProcessDataA();
    }
    """
    file_a = write_test_file("MultiFileA.cpp", file_a_code)
    file_b = write_test_file("MultiFileB.cpp", file_b_code)

    main.parse_cpp_file(file_a, force=True)
    main.parse_cpp_file(file_b, force=True)

    # 1. Visualize call graph for ProcessDataA
    res_graph_str = await main.visualize_call_graph("ProcessDataA", max_depth=3, direction="both")
    res_graph = await parse_json_tool_response(res_graph_str)

    assert res_graph["symbol"] == "ProcessDataA"
    assert len(res_graph["nodes"]) >= 2
    assert len(res_graph["edges"]) >= 1

    # Check Mermaid graph output syntax
    mermaid = res_graph["mermaid"]
    assert mermaid.startswith("graph TD")
    assert "ProcessDataA" in mermaid
    assert "-->" in mermaid

    # 2. Query call graph for MainEntryRunner
    res_runner = await parse_json_tool_response(await main.query_call_graph("MainEntryRunner", max_depth=3, direction="callees"))
    assert res_runner["symbol"] == "MainEntryRunner"
    assert len(res_runner["nodes"]) >= 2
    assert "MainEntryRunner" in res_runner["mermaid"]

    print("Multi-file call graph visualization generated valid JSON node/edge hierarchy and Mermaid graph syntax.")


if __name__ == "__main__":
    pytest.main(["-v", __file__])
