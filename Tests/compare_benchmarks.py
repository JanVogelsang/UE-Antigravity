#!/usr/bin/env python3
# Copyright 2026 AgentFramework. All Rights Reserved.

"""Benchmark Results Comparison Script.

Compares a new benchmark run JSON dataset against the baseline (Tests/benchmark_baseline.json)
and generates a comparison summary report.
"""

import os
import sys
import json
import argparse
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_BASELINE_PATH = SCRIPT_DIR / "benchmark_baseline.json"

def load_json(filepath: Path) -> dict:
    with open(filepath, "r", encoding="utf-8") as f:
        return json.load(f)

def format_delta(current: float, baseline: float, lower_is_better: bool = False, is_pct: bool = False) -> str:
    diff = current - baseline
    if abs(diff) < 1e-6:
        return "0.0" if is_pct else "0"
    
    sign = "+" if diff > 0 else ""
    unit = "%" if is_pct else ""
    val_str = f"{sign}{diff:.1f}{unit}"
    
    # Determine if improvement
    if lower_is_better:
        is_good = diff < 0
    else:
        is_good = diff > 0
        
    indicator = " (Improvement)" if is_good else " (Regression)"
    return f"{val_str}{indicator}"

def compare_benchmarks(baseline_data: dict, current_data: dict) -> str:
    b_sum = baseline_data.get("summary", {})
    c_sum = current_data.get("summary", {})

    report_lines = [
        "# Benchmark Performance Comparison Report",
        "",
        "## Overall Telemetry Metrics",
        "",
        "| Metric | Baseline | Current Run | Delta |",
        "| :--- | :--- | :--- | :--- |",
        f"| **Total Tasks Evaluated** | {b_sum.get('total_tasks', 0)} | {c_sum.get('total_tasks', 0)} | - |",
        f"| **Immediate (First-Try) Pass Rate** | {b_sum.get('immediate_pass_rate_pct', 0.0):.1f}% | {c_sum.get('immediate_pass_rate_pct', 0.0):.1f}% | {format_delta(c_sum.get('immediate_pass_rate_pct', 0.0), b_sum.get('immediate_pass_rate_pct', 0.0), lower_is_better=False, is_pct=True)} |",
        f"| **Self-Corrected Pass Rate** | {b_sum.get('self_corrected_pass_rate_pct', 0.0):.1f}% | {c_sum.get('self_corrected_pass_rate_pct', 0.0):.1f}% | {format_delta(c_sum.get('self_corrected_pass_rate_pct', 0.0), b_sum.get('self_corrected_pass_rate_pct', 0.0), lower_is_better=True, is_pct=True)} |",
        f"| **Failure Rate** | {b_sum.get('fail_rate_pct', 0.0):.1f}% | {c_sum.get('fail_rate_pct', 0.0):.1f}% | {format_delta(c_sum.get('fail_rate_pct', 0.0), b_sum.get('fail_rate_pct', 0.0), lower_is_better=True, is_pct=True)} |",
        f"| **Total Tool Calls** | {b_sum.get('total_tool_calls', 0)} | {c_sum.get('total_tool_calls', 0)} | {format_delta(c_sum.get('total_tool_calls', 0), b_sum.get('total_tool_calls', 0), lower_is_better=True)} |",
        f"| **Avg Tool Calls / Task** | {b_sum.get('avg_tool_calls_per_task', 0.0):.1f} | {c_sum.get('avg_tool_calls_per_task', 0.0):.1f} | {format_delta(c_sum.get('avg_tool_calls_per_task', 0.0), b_sum.get('avg_tool_calls_per_task', 0.0), lower_is_better=True)} |",
        f"| **Total Duration (s)** | {b_sum.get('total_duration_seconds', 0.0):.1f}s | {c_sum.get('total_duration_seconds', 0.0):.1f}s | {format_delta(c_sum.get('total_duration_seconds', 0.0), b_sum.get('total_duration_seconds', 0.0), lower_is_better=True)} |",
        "",
        "## Per-Task Status Comparison",
        "",
        "| Task ID | Target Prompt | Baseline Status | Current Status | Status Shift |",
        "| :--- | :--- | :--- | :--- | :--- |"
    ]

    b_tasks = {t["task_id"]: t for t in baseline_data.get("tasks", [])}
    c_tasks = {t["task_id"]: t for t in current_data.get("tasks", [])}

    all_task_ids = sorted(list(set(b_tasks.keys()).union(set(c_tasks.keys()))))

    for tid in all_task_ids:
        b_t = b_tasks.get(tid, {})
        c_t = c_tasks.get(tid, {})
        
        prompt = c_t.get("prompt") or b_t.get("prompt", "")
        if len(prompt) > 40:
            prompt = prompt[:37] + "..."
            
        b_stat = b_t.get("status", "N/A")
        c_stat = c_t.get("status", "N/A")
        
        if b_stat == c_stat:
            shift = "Unchanged"
        elif c_stat == "IMMEDIATE_PASS" and b_stat != "IMMEDIATE_PASS":
            shift = "PROMOTED to IMMEDIATE_PASS"
        elif c_stat == "FAIL":
            shift = "REGRESSED to FAIL"
        else:
            shift = f"{b_stat} -> {c_stat}"

        report_lines.append(f"| `{tid}` | `{prompt}` | `{b_stat}` | `{c_stat}` | {shift} |")

    return "\n".join(report_lines)

def main():
    parser = argparse.ArgumentParser(description="Compare benchmark JSON run results against baseline.")
    parser.add_argument("--current", type=str, required=True, help="Path to current benchmark JSON report.")
    parser.add_argument("--baseline", type=str, default=str(DEFAULT_BASELINE_PATH), help="Path to baseline JSON report.")
    parser.add_argument("--output", type=str, help="Optional output Markdown file path.")
    
    args = parser.parse_args()
    
    baseline_path = Path(args.baseline)
    current_path = Path(args.current)
    
    if not baseline_path.exists():
        print(f"Error: Baseline file not found at {baseline_path}")
        sys.exit(1)
        
    if not current_path.exists():
        print(f"Error: Current benchmark results file not found at {current_path}")
        sys.exit(1)
        
    baseline_data = load_json(baseline_path)
    current_data = load_json(current_path)
    
    report = compare_benchmarks(baseline_data, current_data)
    
    if args.output:
        out_path = Path(args.output)
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(report)
        print(f"Comparison report saved to {out_path}")
    else:
        print(report)

if __name__ == "__main__":
    main()
