#!/usr/bin/env python3
# Copyright 2026 AgentFramework. All Rights Reserved.

"""Agent Performance and Token Efficiency Benchmarking Framework.

This script executes simulated coding agent tasks against a virtual editor state
and evaluates the performance, token usage, correctness, and rigor using defined rubrics.
It generates a formatted Markdown report indicating the scores.
"""

import os
import sys
import json
import time
import argparse
from datetime import datetime, timezone

# Resolve directories relative to the script location
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
DEFAULT_REPORT_PATH = os.path.join(PROJECT_ROOT, "benchmark_report.md")


class VirtualEditor:
    """Maintains a simulated Unreal Editor state for tracking changes made by tool calls."""

    def __init__(self):
        self.blueprints = {}
        self.search_count = 0
        self.listed_count = 0

    def reset(self):
        """Reset the editor state to initial conditions."""
        self.blueprints.clear()
        self.search_count = 0
        self.listed_count = 0


class MockToolRegistry:
    """Registry and execution engine for mock tools, tracking call history and performance."""

    def __init__(self, editor: VirtualEditor):
        self.editor = editor
        self.history = []

    def execute_tool(self, tool_name: str, args: dict):
        """Simulate execution of a tool and log its performance metrics."""
        start_time = time.time()
        success = False
        message = ""

        # Tool simulation logic
        if tool_name == "create_blueprint":
            name = args.get("name")
            base_class = args.get("base_class", "Actor")
            if not name:
                message = "Error: Blueprint name not specified."
            elif name in self.editor.blueprints:
                message = f"Error: Blueprint {name} already exists."
            else:
                self.editor.blueprints[name] = {
                    "base_class": base_class,
                    "variables": {},
                    "compiled": False
                }
                success = True
                message = f"Successfully created blueprint {name} inheriting from {base_class}."

        elif tool_name == "add_variable":
            blueprint_name = args.get("blueprint_name")
            var_name = args.get("var_name")
            var_type = args.get("var_type", "float")

            if not blueprint_name or not var_name:
                message = "Error: blueprint_name and var_name must be specified."
            elif blueprint_name not in self.editor.blueprints:
                message = f"Error: Blueprint {blueprint_name} does not exist."
            elif var_name in self.editor.blueprints[blueprint_name]["variables"]:
                message = f"Error: Variable {var_name} already exists in {blueprint_name}."
            else:
                self.editor.blueprints[blueprint_name]["variables"][var_name] = var_type
                success = True
                message = f"Successfully added variable {var_name} ({var_type}) to blueprint {blueprint_name}."

        elif tool_name == "compile_blueprint":
            name = args.get("name")
            if not name:
                message = "Error: Blueprint name not specified."
            elif name not in self.editor.blueprints:
                message = f"Error: Blueprint {name} does not exist."
            else:
                self.editor.blueprints[name]["compiled"] = True
                success = True
                message = f"Successfully compiled blueprint {name}."

        elif tool_name == "search_assets":
            query = args.get("query", "")
            self.editor.search_count += 1
            matches = [bp for bp in self.editor.blueprints if query.lower() in bp.lower()]
            success = True
            message = f"Found {len(matches)} matching assets: {matches}."

        elif tool_name == "list_blueprints":
            self.editor.listed_count += 1
            success = True
            message = f"Available blueprints: {list(self.editor.blueprints.keys())}."

        else:
            message = f"Error: Unknown tool '{tool_name}'."

        # Simulate small delay for executing the tool logic (e.g. 10ms - 50ms)
        time.sleep(0.02)
        duration = time.time() - start_time

        log_entry = {
            "timestamp": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
            "tool_name": tool_name,
            "args": args,
            "success": success,
            "duration": duration,
            "message": message
        }
        self.history.append(log_entry)
        return success, message, duration


class BenchmarkRunner:
    """Benchmark runner implementing evaluations against Rigor, Token Efficiency, Correctness."""

    def __init__(self):
        self.editor = VirtualEditor()
        self.registry = MockToolRegistry(self.editor)
        self.results = []

    def run_task(self, task_def: dict, verbose: bool = False):
        """Execute a simulated task definition and measure all metrics."""
        if verbose:
            print(f"==================================================")
            print(f"Running Task: {task_def['name']}")
            print(f"Description: {task_def['description']}")
            print(f"==================================================")

        self.editor.reset()
        self.registry.history.clear()

        start_time = time.time()
        total_prompt_tokens = 0
        total_response_tokens = 0

        # Simulate step-by-step agent loop
        for i, step in enumerate(task_def["steps"]):
            prompt_toks = step.get("prompt_tokens", 0)
            resp_toks = step.get("response_tokens", 0)
            total_prompt_tokens += prompt_toks
            total_response_tokens += resp_toks

            if verbose:
                print(f"[Step {i+1}] Prompt: '{step.get('thought', 'Thinking...')}'")
                print(f"         Estimated Prompt Tokens: {prompt_toks}, Response Tokens: {resp_toks}")

            if "tool" in step:
                tool_name = step["tool"]["name"]
                tool_args = step["tool"]["args"]
                success, msg, dur = self.registry.execute_tool(tool_name, tool_args)
                if verbose:
                    status_str = "SUCCESS" if success else "FAILED"
                    print(f"         Tool Call: `{tool_name}({json.dumps(tool_args)})` -> {status_str} ({dur:.3f}s)")
                    print(f"         Response: {msg}")

        duration = time.time() - start_time

        metrics = {
            "duration": duration,
            "tool_calls_count": len(self.registry.history),
            "prompt_tokens": total_prompt_tokens,
            "response_tokens": total_response_tokens,
            "total_tokens": total_prompt_tokens + total_response_tokens
        }

        # Calculate Rubrics
        scores = self.evaluate_rubrics(task_def, metrics, self.registry.history)
        overall_score = sum(scores.values()) / len(scores)

        task_result = {
            "name": task_def["name"],
            "description": task_def["description"],
            "metrics": metrics,
            "scores": scores,
            "overall_score": overall_score,
            "history": list(self.registry.history),
            "passed": overall_score >= task_def.get("passing_score", 70.0)
        }

        self.results.append(task_result)
        if verbose:
            print(f"\nTask Results:")
            for rubric, score in scores.items():
                print(f"  - {rubric}: {score:.1f}%")
            print(f"  - Overall Score: {overall_score:.1f}% (Passing: {task_def.get('passing_score', 70.0)}%)")
            print(f"  - Status: {'PASS' if task_result['passed'] else 'FAIL'}")
            print(f"==================================================\n")

        return task_result

    def evaluate_rubrics(self, task_def: dict, metrics: dict, history: list):
        """Run all evaluation scoring functions based on rubrics."""
        return {
            "Correctness": self.evaluate_correctness(task_def, self.editor.blueprints, history),
            "Token_Efficiency": self.evaluate_efficiency(task_def, metrics),
            "Performance": self.evaluate_performance(task_def, metrics),
            "Rigor": self.evaluate_rigor(task_def, history)
        }

    def evaluate_correctness(self, task_def: dict, blueprints: dict, history: list) -> float:
        """Evaluate task correctness based on expected target assets and variables."""
        rules = task_def.get("validation_rules", [])
        if not rules:
            return 100.0

        passed = 0
        for rule in rules:
            rule_type = rule.get("type")
            if rule_type == "blueprint_exists":
                bp_name = rule.get("name")
                if bp_name in blueprints:
                    passed += 1
            elif rule_type == "variable_exists":
                bp_name = rule.get("blueprint_name")
                var_name = rule.get("variable_name")
                var_type = rule.get("variable_type")
                bp = blueprints.get(bp_name)
                if bp and var_name in bp.get("variables", {}):
                    if not var_type or bp["variables"][var_name] == var_type:
                        passed += 1
            elif rule_type == "compiled":
                bp_name = rule.get("blueprint_name")
                bp = blueprints.get(bp_name)
                if bp and bp.get("compiled", False):
                    passed += 1

        return (passed / len(rules)) * 100.0

    def evaluate_efficiency(self, task_def: dict, metrics: dict) -> float:
        """Evaluate token and API tool call efficiency compared to task targets."""
        target_tool_calls = task_def.get("target_tool_calls", 3)
        target_tokens = task_def.get("target_tokens", 2000)

        actual_calls = metrics.get("tool_calls_count", 0)
        actual_tokens = metrics.get("total_tokens", 0)

        # Tool calls score
        if actual_calls <= target_tool_calls:
            call_score = 100.0
        else:
            excess_calls = actual_calls - target_tool_calls
            call_score = max(0.0, 100.0 - (excess_calls * 20.0))

        # Token usage score
        if actual_tokens <= target_tokens:
            token_score = 100.0
        else:
            excess_tokens = actual_tokens - target_tokens
            token_score = max(0.0, 100.0 - (excess_tokens / 1000.0) * 10.0)

        return (call_score + token_score) / 2.0

    def evaluate_performance(self, task_def: dict, metrics: dict) -> float:
        """Evaluate task execution time compared to a performance threshold."""
        target_duration = task_def.get("target_duration", 5.0)
        actual_duration = metrics.get("duration", 0.0)

        if actual_duration <= target_duration:
            return 100.0
        else:
            excess = actual_duration - target_duration
            return max(0.0, 100.0 - (excess * 15.0))

    def evaluate_rigor(self, task_def: dict, history: list) -> float:
        """Evaluate agent safety/rigor: did it compile, and did any tool calls fail?"""
        score = 0.0

        # Rule 1: Did it run compilation/verification tools to verify its work?
        verification_tools = ["compile_blueprint", "verify_integrity", "run_tests"]
        has_verification = any(log.get("tool_name") in verification_tools for log in history)
        if has_verification:
            score += 50.0

        # Rule 2: Did it complete all tool operations without a tool failing?
        has_failure = any(not log.get("success", False) for log in history)
        if not has_failure and len(history) > 0:
            score += 50.0

        return score

    def generate_report(self, report_path: str = DEFAULT_REPORT_PATH):
        """Format and write the benchmark markdown report to the designated path."""
        total_tasks = len(self.results)
        passed_tasks = sum(1 for r in self.results if r["passed"])
        success_rate = (passed_tasks / total_tasks) * 100.0 if total_tasks > 0 else 0.0
        total_duration = sum(r["metrics"]["duration"] for r in self.results)

        lines = [
            "# Agent Benchmark Performance & Token Efficiency Report",
            "",
            f"- **Timestamp (UTC):** {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M:%S')}Z",
            f"- **Total Tasks Evaluated:** {total_tasks}",
            f"- **Passed Tasks:** {passed_tasks}",
            f"- **Success Rate:** {success_rate:.1f}%",
            f"- **Total Run Duration:** {total_duration:.3f} seconds",
            "",
            "## Rubric Score Card Summary",
            "",
            "| Task Name | Correctness | Token Efficiency | Performance | Rigor | Overall Score | Status |",
            "|---|---|---|---|---|---|---|",
        ]

        for res in self.results:
            scores = res["scores"]
            status = "PASS" if res["passed"] else "FAIL"
            lines.append(
                f"| {res['name']} | {scores['Correctness']:.1f}% | {scores['Token_Efficiency']:.1f}% | "
                f"{scores['Performance']:.1f}% | {scores['Rigor']:.1f}% | **{res['overall_score']:.1f}%** | {status} |"
            )

        lines.append("")
        lines.append("## Detailed Task Evaluations")
        lines.append("")

        for res in self.results:
            lines.extend([
                f"### Task: {res['name']}",
                f"*{res['description']}*",
                "",
                "#### Metrics:",
                f"- **Execution Time:** {res['metrics']['duration']:.3f} seconds",
                f"- **Tool Calls Made:** {res['metrics']['tool_calls_count']}",
                f"- **Tokens Used:** {res['metrics']['total_tokens']} (Prompt: {res['metrics']['prompt_tokens']}, Response: {res['metrics']['response_tokens']})",
                "",
                "#### Rubric Score Breakdown:",
            ])
            for rubric, score in res["scores"].items():
                lines.append(f"- **{rubric.replace('_', ' ')}:** {score:.1f}%")

            lines.extend([
                "",
                "#### Execution Tool Log:",
                "",
                "| Timestamp | Tool Name | Arguments | Success | Duration | Message |",
                "|---|---|---|---|---|---|",
            ])
            for log in res["history"]:
                args_str = json.dumps(log["args"])
                lines.append(
                    f"| {log['timestamp']} | `{log['tool_name']}` | `{args_str}` | {log['success']} | "
                    f"{log['duration']:.3f}s | {log['message']} |"
                )
            lines.extend(["", "---", ""])

        report_content = "\n".join(lines)

        os.makedirs(os.path.dirname(report_path), exist_ok=True)
        with open(report_path, "w", encoding="utf-8") as f:
            f.write(report_content)

        print(f"Benchmark report generated successfully at: {report_path}")


# Predefined task definitions to simulate various scenarios
TASKS = [
    {
        "name": "Create Character Blueprint with Variable",
        "description": "Mock simulation of creating a player character blueprint, adding a speed variable, and compiling it.",
        "passing_score": 75.0,
        "target_tool_calls": 3,
        "target_tokens": 1200,
        "target_duration": 1.0,
        "steps": [
            {
                "thought": "First, create the blueprint representing the player character.",
                "prompt_tokens": 150,
                "response_tokens": 50,
                "tool": {
                    "name": "create_blueprint",
                    "args": {"name": "BP_HeroCharacter", "base_class": "Character"}
                }
            },
            {
                "thought": "Next, add the Speed variable of type float.",
                "prompt_tokens": 200,
                "response_tokens": 45,
                "tool": {
                    "name": "add_variable",
                    "args": {"blueprint_name": "BP_HeroCharacter", "var_name": "Speed", "var_type": "float"}
                }
            },
            {
                "thought": "Compile the blueprint to verify it has no errors.",
                "prompt_tokens": 220,
                "response_tokens": 35,
                "tool": {
                    "name": "compile_blueprint",
                    "args": {"name": "BP_HeroCharacter"}
                }
            }
        ],
        "validation_rules": [
            {"type": "blueprint_exists", "name": "BP_HeroCharacter"},
            {"type": "variable_exists", "blueprint_name": "BP_HeroCharacter", "variable_name": "Speed", "variable_type": "float"},
            {"type": "compiled", "blueprint_name": "BP_HeroCharacter"}
        ]
    },
    {
        "name": "Failing Task - Blueprint Missing",
        "description": "Simulates an agent trying to add a variable to a blueprint that doesn't exist, leading to a tool failure.",
        "passing_score": 70.0,
        "target_tool_calls": 2,
        "target_tokens": 1000,
        "target_duration": 1.0,
        "steps": [
            {
                "thought": "I will try to directly add the variable without creating the blueprint first.",
                "prompt_tokens": 120,
                "response_tokens": 30,
                "tool": {
                    "name": "add_variable",
                    "args": {"blueprint_name": "BP_NonExistent", "var_name": "Health", "var_type": "float"}
                }
            }
        ],
        "validation_rules": [
            {"type": "blueprint_exists", "name": "BP_NonExistent"}
        ]
    },
    {
        "name": "Inefficient Agent Search and List",
        "description": "Simulates an agent using multiple redundant search and list queries before executing the final creation step.",
        "passing_score": 70.0,
        "target_tool_calls": 2,
        "target_tokens": 800,
        "target_duration": 1.0,
        "steps": [
            {
                "thought": "Search for the blueprint BP_NewActor to see if it already exists.",
                "prompt_tokens": 200,
                "response_tokens": 40,
                "tool": {
                    "name": "search_assets",
                    "args": {"query": "BP_NewActor"}
                }
            },
            {
                "thought": "Let's perform another search to be absolutely certain.",
                "prompt_tokens": 400,
                "response_tokens": 50,
                "tool": {
                    "name": "search_assets",
                    "args": {"query": "BP_NewActor"}
                }
            },
            {
                "thought": "List all blueprints in the project just to double-check.",
                "prompt_tokens": 600,
                "response_tokens": 80,
                "tool": {
                    "name": "list_blueprints",
                    "args": {}
                }
            },
            {
                "thought": "Okay, the blueprint is not there. I will now create it.",
                "prompt_tokens": 800,
                "response_tokens": 50,
                "tool": {
                    "name": "create_blueprint",
                    "args": {"name": "BP_NewActor", "base_class": "Actor"}
                }
            }
        ],
        "validation_rules": [
            {"type": "blueprint_exists", "name": "BP_NewActor"}
        ]
    }
]


def main():
    parser = argparse.ArgumentParser(description="Run performance and token efficiency benchmarks.")
    parser.add_argument(
        "-r", "--report",
        default=DEFAULT_REPORT_PATH,
        help="Path where the markdown report should be written."
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Print verbose execution logging to console."
    )
    parser.add_argument(
        "-t", "--task",
        choices=[t["name"] for t in TASKS],
        help="Run only a specific task."
    )

    args = parser.parse_args()

    runner = BenchmarkRunner()

    tasks_to_run = TASKS
    if args.task:
        tasks_to_run = [t for t in TASKS if t["name"] == args.task]

    print(f"Starting Benchmark Run on {len(tasks_to_run)} task(s)...")

    for task in tasks_to_run:
        runner.run_task(task, verbose=args.verbose or True)

    runner.generate_report(args.report)

    # Print summary table to stdout
    print("\n" + "=" * 80)
    print(f"BENCHMARK SUMMARY CARD (Saved to {args.report})")
    print("=" * 80)
    print(f"{'Task Name':<35} | {'Corr.':<6} | {'Eff.':<6} | {'Perf.':<6} | {'Rigor':<6} | {'Overall':<7} | {'Status':<6}")
    print("-" * 80)
    for res in runner.results:
        s = res["scores"]
        status = "PASS" if res["passed"] else "FAIL"
        print(f"{res['name']:<35} | {s['Correctness']:>5.1f}% | {s['Token_Efficiency']:>5.1f}% | {s['Performance']:>5.1f}% | {s['Rigor']:>5.1f}% | {res['overall_score']:>6.1f}% | {status:<6}")
    print("=" * 80 + "\n")


if __name__ == "__main__":
    main()
