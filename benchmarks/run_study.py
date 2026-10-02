"""Repeatable diagnostic launcher; synthetic mode is NEVER presented as market evidence."""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import os
import platform
import statistics
import subprocess
import sys
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCES = ["CMakeLists.txt", "include/metalarch/core.hpp", "include/metalarch/engines.hpp", "include/metalarch/legacy_extensions.hpp",
           "src/core.cpp", "src/engines.cpp", "src/legacy_extensions.cpp", "src/main.cpp", "benchmarks/compare.cpp",
           "benchmarks/bench.cpp", "benchmarks/run_study.py",
           "benchmarks/run_budget_sweep.py", "benchmarks/plot_results.py", "fixtures/make_fixture.py", "tests/test_core.cpp", "tests/test_legacy.cpp"]


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def capture(*args: str) -> str:
    try:
        return subprocess.check_output(args, cwd=ROOT, text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unavailable"


def quantile(values: list[float], q: float) -> float:
    values = sorted(values)
    if not values:
        return 0.0
    pos = (len(values) - 1) * q
    low = int(pos)
    hi = min(low + 1, len(values) - 1)
    return values[low] + (values[hi] - values[low]) * (pos - low)


def main() -> None:
    parser = argparse.ArgumentParser(description="Run all five MetalArch diagnostic policies")
    parser.add_argument("--trace", type=Path, help="Existing MA1 trace (default: generate synthetic)")
    parser.add_argument("--cohort", choices=("core", "expanded"), default="expanded",
                        help="11-stage original native graph or 19-stage ASEP2-expanded workload")
    parser.add_argument("--bars", type=int, default=300)
    parser.add_argument("--warmup-bars", type=int, default=65)
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--budget-ns", type=int, default=8_000_000,
                        help="DECLARED nominal cost-estimate budget, not actual CPU time")
    parser.add_argument("--exe", type=Path, default=ROOT / "build" / "metalarch_compare")
    parser.add_argument("--outdir", type=Path, default=ROOT / "artifacts" / "diagnostic")
    args = parser.parse_args()
    args.outdir.mkdir(parents=True, exist_ok=True)
    trace = args.trace.resolve() if args.trace else args.outdir / f"synthetic_{args.bars}.ma1"
    if not args.trace:
        subprocess.run([sys.executable, str(ROOT / "fixtures" / "make_fixture.py"),
                        "--bars", str(args.bars), "--output", str(trace)], check=True, cwd=ROOT,
                       capture_output=True, text=True)
    exe = args.exe.resolve()
    if not exe.is_file():
        parser.error(f"build comparison executable first: {exe}")
    raw_path = args.outdir / "events.csv"
    command = [str(exe), str(trace), str(raw_path), str(args.runs),
               str(args.warmup_bars), str(args.budget_ns), args.cohort]
    completed = subprocess.run(command, check=True, cwd=ROOT, capture_output=True, text=True)
    (args.outdir / "per_run.txt").write_text(completed.stdout, encoding="utf-8")
    rows = list(csv.DictReader(raw_path.open(encoding="utf-8", newline="")))
    groups: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        groups[row["policy"]].append(row)
    summary = {}
    for policy, vals in sorted(groups.items()):
        times = [float(r["wall_us"]) for r in vals]
        run_p95 = [quantile([float(r["wall_us"]) for r in vals if r["run"] == str(i)], .95)
                   for i in range(args.runs)]
        summary[policy] = {
            "samples": len(vals), "wall_p50_us": quantile(times, .5),
            "wall_p95_us": quantile(times, .95), "wall_p99_us": quantile(times, .99),
            "median_across_run_p95_us": statistics.median(run_p95),
            "cpu_total_us": sum(float(r["process_cpu_us"]) for r in vals),
            "computations": sum(int(r["computations"]) for r in vals),
            "cache_hits": sum(int(r["hits"]) for r in vals),
            "deferred": sum(int(r["deferred"]) for r in vals),
            "status_mismatches_vs_B0": sum(int(r["status_mismatches"]) for r in vals),
            "unflagged_age_violations": sum(int(r["unflagged_age_violations"]) for r in vals),
            "counts": {label: sum(int(r[label]) for r in vals)
                       for label in ("valid", "stale", "unavailable", "failed")},
            "max_common_valid_abs_error": max(float(r["max_common_valid_abs_error"]) for r in vals),
        }
    manifest = {
        "kind": "LOCAL_SYNTHETIC_DIAGNOSTIC_NOT_PUBLICATION_FINDING",
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "git_head": capture("git", "rev-parse", "HEAD"),
        "git_dirty": capture("git", "status", "--porcelain"),
        "source_sha256": {name: sha256(ROOT / name) for name in SOURCES},
        "cohort": args.cohort, "registered_stages": 19 if args.cohort=="expanded" else 11,
        "trace_path": str(trace), "trace_sha256": sha256(trace),
        "csv_sha256": sha256(raw_path), "binary_sha256": sha256(exe),
        "platform": platform.platform(), "cpu": platform.processor(),
        "cpu_count": os.cpu_count(), "python": platform.python_version(),
        "cxx_version": capture("c++", "--version").splitlines()[0],
        "cmake_version": capture("cmake", "--version").splitlines()[0],
        "policies": ["B0_full", "B1_parallel", "B2_cache", "B3_cadence", "P_freshness"],
        "assumptions": ["source mode SIMULATED for generated trace",
                        "local ingestion plus execution only; no network latency",
                        "no CPU pinning, frequency or thermal control",
                        "P uses declared initial estimated costs, not empirical calibrated service times",
                        "reference checked per event for correctness baselines"],
        "command": command, "warmup_bars": args.warmup_bars, "runs": args.runs,
        "budget_nominal_ns": args.budget_ns,
    }
    (args.outdir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    (args.outdir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    for policy, d in summary.items():
        print(f"{policy:12} p50={d['wall_p50_us']:.2f}us p95={d['wall_p95_us']:.2f}us "
              f"computations={d['computations']} deferred={d['deferred']} mismatches={d['status_mismatches_vs_B0']}")
    print(f"Artifacts: {args.outdir}")


if __name__ == "__main__":
    main()
