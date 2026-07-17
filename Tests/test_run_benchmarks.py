# Copyright 2026 AgentFramework. All Rights Reserved.

import os
import sys
import tempfile
import pytest
from pathlib import Path

# Add UnrealEngine folder and scripts folder to sys.path
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
sys.path.append(str(PROJECT_ROOT / "UnrealEngine" / "src" / "scripts"))

# Import from run_benchmarks
from run_benchmarks import VirtualEditor, MockToolRegistry, BenchmarkRunner, TASKS


def test_virtual_editor_lifecycle():
    editor = VirtualEditor()
    assert len(editor.blueprints) == 0
    assert editor.search_count == 0
    assert editor.listed_count == 0

    editor.blueprints["BP_Test"] = {"base_class": "Actor", "variables": {}, "compiled": False}
    editor.search_count = 5
    editor.listed_count = 3

    editor.reset()
    assert len(editor.blueprints) == 0
    assert editor.search_count == 0
    assert editor.listed_count == 0


def test_mock_tool_registry_create_blueprint():
    editor = VirtualEditor()
    registry = MockToolRegistry(editor)

    # Missing name
    success, message, _ = registry.execute_tool("create_blueprint", {})
    assert success is False
    assert "Error" in message

    # Valid creation
    success, message, _ = registry.execute_tool("create_blueprint", {"name": "BP_Player", "base_class": "Character"})
    assert success is True
    assert "BP_Player" in editor.blueprints
    assert editor.blueprints["BP_Player"]["base_class"] == "Character"

    # Duplicate creation
    success, message, _ = registry.execute_tool("create_blueprint", {"name": "BP_Player"})
    assert success is False
    assert "already exists" in message


def test_mock_tool_registry_add_variable():
    editor = VirtualEditor()
    registry = MockToolRegistry(editor)

    # Target blueprint does not exist
    success, message, _ = registry.execute_tool("add_variable", {"blueprint_name": "BP_NonExistent", "var_name": "Health"})
    assert success is False
    assert "does not exist" in message

    # Create blueprint first
    registry.execute_tool("create_blueprint", {"name": "BP_Player"})
    
    # Add variable successfully
    success, message, _ = registry.execute_tool("add_variable", {"blueprint_name": "BP_Player", "var_name": "Health", "var_type": "float"})
    assert success is True
    assert "Health" in editor.blueprints["BP_Player"]["variables"]
    assert editor.blueprints["BP_Player"]["variables"]["Health"] == "float"

    # Duplicate variable
    success, message, _ = registry.execute_tool("add_variable", {"blueprint_name": "BP_Player", "var_name": "Health"})
    assert success is False
    assert "already exists" in message


def test_mock_tool_registry_compile_blueprint():
    editor = VirtualEditor()
    registry = MockToolRegistry(editor)

    # Missing blueprint compile
    success, message, _ = registry.execute_tool("compile_blueprint", {"name": "BP_Player"})
    assert success is False

    # Create and compile
    registry.execute_tool("create_blueprint", {"name": "BP_Player"})
    success, message, _ = registry.execute_tool("compile_blueprint", {"name": "BP_Player"})
    assert success is True
    assert editor.blueprints["BP_Player"]["compiled"] is True


def test_benchmark_runner_scoring():
    runner = BenchmarkRunner()
    task = TASKS[0]  # Success scenario

    res = runner.run_task(task, verbose=False)
    assert res["name"] == task["name"]
    assert res["passed"] is True
    assert res["scores"]["Correctness"] == 100.0
    assert res["scores"]["Token_Efficiency"] == 100.0  # Under targets
    assert res["scores"]["Rigor"] == 100.0  # Compiled, no errors
    assert res["overall_score"] == 100.0


def test_benchmark_runner_scoring_failure():
    runner = BenchmarkRunner()
    task = TASKS[1]  # Failure scenario

    res = runner.run_task(task, verbose=False)
    assert res["name"] == task["name"]
    assert res["passed"] is False
    assert res["scores"]["Correctness"] == 0.0
    assert res["scores"]["Rigor"] == 0.0  # No compile, has tool failures


def test_benchmark_runner_scoring_inefficient():
    runner = BenchmarkRunner()
    task = TASKS[2]  # Inefficient scenario

    res = runner.run_task(task, verbose=False)
    assert res["name"] == task["name"]
    # Should have lower Token_Efficiency due to extra tool calls & high token counts
    assert res["scores"]["Token_Efficiency"] < 100.0


def test_report_generation():
    runner = BenchmarkRunner()
    for task in TASKS:
        runner.run_task(task, verbose=False)

    with tempfile.TemporaryDirectory() as temp_dir:
        report_path = os.path.join(temp_dir, "test_report.md")
        runner.generate_report(report_path)
        assert os.path.exists(report_path)
        
        with open(report_path, "r", encoding="utf-8") as f:
            content = f.read()
            assert "# Agent Benchmark Performance & Token Efficiency Report" in content
            assert "## Rubric Score Card Summary" in content
            assert "BP_HeroCharacter" in content
