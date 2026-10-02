"""Synthetic, disjoint-trace profiling/calibration pilot; never a production benchmark.

Generates earlier CALIBRATION events and later EVALUATION events, fits a frozen
per-engine cost table using B0 only, then runs the existing five-policy harness.
No runtime measurements from held-out events influence P's admission costs.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import os
import platform
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(*argv: str):
    subprocess.run(argv, check=True, cwd=ROOT)


def cgroup(name: str) -> str | None:
    target = Path('/sys/fs/cgroup') / name
    try:
        return target.read_text(encoding='utf-8').strip()
    except OSError:
        return None


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument('--calibration-bars', type=int, default=120)
    p.add_argument('--evaluation-bars', type=int, default=140)
    p.add_argument('--evaluation-start-index', type=int, default=2001)
    p.add_argument('--warmup-bars', type=int, default=65)
    p.add_argument('--runs', type=int, default=3)
    p.add_argument('--cohort', choices=['core', 'expanded'], default='expanded')
    p.add_argument('--declared-budget-ns', type=int, default=8_000_000)
    p.add_argument('--calibrated-budget-fraction', type=float, default=.65)
    p.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    p.add_argument('--outdir', type=Path, default=ROOT / 'artifacts' / 'm1_study')
    args = p.parse_args()
    if args.calibration_bars < args.warmup_bars + 5 or args.evaluation_bars < args.warmup_bars + 5:
        p.error('Both traces need at least warmup-bars + 5 bars')
    if args.evaluation_start_index <= args.calibration_bars:
        p.error('Evaluation indices must be strictly later than the calibration interval')
    if not 0 < args.calibrated_budget_fraction <= 1:
        p.error('calibrated-budget-fraction must be in (0, 1]')
    if args.runs < 1:
        p.error('runs must be positive')
    output = args.outdir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    train = output / 'calibration_previous.ma1'
    heldout = output / 'evaluation_heldout.ma1'
    generate = str(ROOT / 'fixtures' / 'make_fixture.py')
    run(sys.executable, generate, '--bars', str(args.calibration_bars), '--start-index', '1', '--output', str(train))
    run(sys.executable, generate, '--bars', str(args.evaluation_bars),
        '--start-index', str(args.evaluation_start_index), '--output', str(heldout))
    if sha(train) == sha(heldout):
        raise RuntimeError('Calibration and evaluation input hashes unexpectedly match')
    build = args.build_dir.resolve()
    calibrator = str(build / 'metalarch_calibrate')
    comparison = str(build / 'metalarch_compare')
    cost_table = output / 'frozen_costs.tsv'
    run(calibrator, str(train), str(cost_table), args.cohort, str(args.warmup_bars * 3), '5')
    with cost_table.open(encoding='utf-8', newline='') as f:
        costs = list(csv.DictReader(f, delimiter='\t'))
    if len(costs) != (19 if args.cohort == 'expanded' else 11):
        raise RuntimeError('Missing calibration rows')
    total_frozen_ns = sum(int(r['cost_ns']) for r in costs)
    budget_frozen = max(1, int(total_frozen_ns * args.calibrated_budget_fraction))
    runner = str(ROOT / 'benchmarks' / 'run_study.py')
    studies = (
        ('declared_unprofiled', args.declared_budget_ns, False, None),
        ('declared_profiled', args.declared_budget_ns, True, None),
        ('frozen_unprofiled', budget_frozen, False, cost_table),
        ('frozen_profiled', budget_frozen, True, cost_table),
    )
    results = {}
    for label, budget, profile, table in studies:
        cmd = [sys.executable, runner, '--trace', str(heldout), '--exe', comparison,
               '--outdir', str(output / label), '--cohort', args.cohort,
               '--runs', str(args.runs), '--warmup-bars', str(args.warmup_bars),
               '--budget-ns', str(budget)]
        if profile:
            cmd.append('--profile')
        if table:
            cmd += ['--frozen-cost-table', str(table)]
        run(*cmd)
        results[label] = json.loads((output / label / 'summary.json').read_text(encoding='utf-8'))
    # RSS in metalarch_compare includes its entire reference-results history.
    # Run one policy/session per fresh OS process, without an in-memory B0 oracle.
    isolated = {}
    for policy in ('b0', 'b1', 'b2', 'b3', 'p'):
        csv_path = output / f'isolated_{policy}.csv'
        cmd = [str(build / 'metalarch_profile_session'), str(heldout), str(csv_path),
               args.cohort, policy, str(budget_frozen)]
        if policy == 'p':
            cmd.append(str(cost_table))
        run(*cmd)
        with csv_path.open(encoding='utf-8', newline='') as handle:
            samples = list(csv.DictReader(handle))
        if len(samples) != args.evaluation_bars * 3:
            raise RuntimeError('Unexpected single-session profile event count')
        isolated[policy] = {
            'csv_sha256': sha(csv_path), 'events': len(samples),
            'process_peak_rss_bytes': max((int(x['process_peak_rss_bytes']) for x in samples
                                           if x['process_peak_rss_bytes']), default=None),
            'last_current_rss_bytes': next((int(x['rss_after_bytes']) for x in reversed(samples)
                                            if x['rss_after_bytes']), None),
            'reserved_event_payload_bytes_lower_bound': int(samples[-1]['reserved_event_payload_bytes_lower_bound']),
            'computed_calls': sum(int(x['compute_count']) for x in samples),
            'deferrals': sum(int(x['deferred']) for x in samples),
        }
    diagnostic = {
        'status': 'SYNTHETIC_DISJOINT_TRACE_M1_DIAGNOSTIC_NOT_PAPER_FINDING',
        'timestamp_utc': datetime.now(timezone.utc).isoformat(),
        'source_validation': 'calibration and evaluation are distinct, nonoverlapping deterministic simulated index ranges',
        'calibration_indices': [1, args.calibration_bars],
        'evaluation_indices': [args.evaluation_start_index, args.evaluation_start_index + args.evaluation_bars - 1],
        'calibration_trace_sha256': sha(train),
        'evaluation_trace_sha256': sha(heldout),
        'frozen_cost_table_sha256': sha(cost_table),
        'cohort': args.cohort,
        'stages': len(costs),
        'frozen_cost_total_ns': total_frozen_ns,
        'frozen_budget_ns': budget_frozen,
        'declared_budget_nominal_ns': args.declared_budget_ns,
        'calibrated_budget_fraction': args.calibrated_budget_fraction,
        'profile_overhead_warning': 'Separate sequential batches, not thermally controlled. Inspect paired per-run series before inferring overhead.',
        'memory_warning': 'RSS is process-wide; peak RSS is a process-lifetime high-water mark, not per-engine allocation or an enforced limit.',
        'cost_warning': 'Frozen table contains p95 compute-wall-time plus an admission guard, not a CPU-quota guarantee.',
        'host': platform.platform(), 'cpu_count': os.cpu_count(),
        'cgroup_cpu_max': cgroup('cpu.max'), 'cgroup_memory_max': cgroup('memory.max'),
        'profiled': {
            label: {
                policy: {
                    k: record[k] for k in ('wall_p50_us', 'wall_p95_us', 'computations',
                                             'deferred', 'counts', 'peak_process_rss_bytes',
                                             'thread_cpu_samples', 'status_mismatches_vs_B0')
                }
                for policy, record in summaries.items()
            } for label, summaries in results.items()
        },
        'isolated_single_session': isolated,
    }
    (output / 'm1_report.json').write_text(json.dumps(diagnostic, indent=2) + '\n', encoding='utf-8')
    print('M1 research diagnostic:', output / 'm1_report.json')
    print('CAL/HELDOUT trace hashes:', diagnostic['calibration_trace_sha256'], diagnostic['evaluation_trace_sha256'])
    print('FROZEN COST sum/budget:', total_frozen_ns, budget_frozen)


if __name__ == '__main__':
    main()
