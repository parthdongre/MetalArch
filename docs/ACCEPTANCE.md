# MetalArch — Acceptance and Benchmark Contract (PROPOSED v0)

Date: 2026-10-02. These are **planned test criteria**, not current test outcomes. No MetalArch performance targets are reported as achieved.

## Correctness gates (required before performance claims)

| ID | Acceptance criterion | Evidence to collect |
|---|---|---|
| A01 | Duplicate IDs, unknown required dependencies, and DAG cycles are rejected at startup with a useful error. Every scheduled node starts after all required parent results for its input cut have finished. | Unit/property tests, execution order traces |
| A02 | Source updates affecting declared windows, OHLCV fields, peer, book, external feeds, engine parameters, implementation version or upstream outputs change the appropriate dependent cache identity. Unchanged input cuts can hit the cache. | Mutation matrix and property tests |
| A03 | Executions in different sessions/symbols/timeframes/parameter sets cannot share mutable source, cache, scheduler, counter or RNG state by accident. | Interleaved-session isolation tests |
| A04 | Under identical applicable input cuts, B1 and B2 produce the same normalized valid output values/statuses as uncached B0, within declared per-engine numeric tolerances. Incorrect cache reuse count must be **zero** in the validation fixtures. | Golden and differential tests |
| A05 | An unavailable/failed/stale dependency is never silently replaced with score 50 or represented as VALID. Fusion declares how invalid/optional inputs affect coverage. | Fault injection and lineage assertions |
| A06 | Every presented output identifies input cut, engine/parameter versions, required parents, source provenance, computation time, source time, and VALID/STALE/UNAVAILABLE/FAILED status. Observed and synthetic sources are distinguishable. | JSON schema validation and UI/API tests |
| A07 | Recorded-input replay uses the same ingestion, snapshot, DAG and policy implementations; accounted events equal recorded events after declared reject/dedupe policy; normalized output sequence/status and hashes/numbers agree in repeat runs under controlled settings. | Repeated replay + diff report |
| A08 | Duplicates, out-of-order events, source delays, disconnects, engine faults and load bursts follow explicit deterministic policy; queues are bounded and any drops/coalescing are counted. | Fault and overload tests |
| A09 | No broad exception handler swallows assertions in the new test suite; unexpected engine failures are test failures unless that exact failure is under test. | CI tests + adversarial failing fixture |
| A10 | Documentation/paper figures and counts are linked to the exact MetalArch commit and experiment artifacts. No planned work is presented as completed. | Claims ledger + release review |

For deterministic Python-only pure engines on the same software platform, exact canonicalized equality should be attempted first. For floating point/native kernels, predeclare suitable absolute/relative tolerances per engine (and random seeds for stochastic engines); do not choose tolerances retrospectively to hide divergences. Include NaN, missing data and timestamp comparisons. Wall-clock profiling telemetry is not part of deterministic result equality.

## Freshness contract

For each engine, define a freshness deadline and a declared source-age policy **before evaluation**. Store both source-event age (evaluation clock minus underlying input event time) and compute/result age (evaluation clock minus result computation time). A result with matching cache identity can still expire by age. A result for an old input cut may be displayed as explicitly STALE/last-known, but must not impersonate the current VALID result.

An overloaded scheduler may intentionally defer work; this is not a correctness failure if the state and age are disclosed. Unflagged stale presentation is a release blocker. Deadline violation rates, budget misses and fidelity loss are measured outcomes, not suppressed.

## Benchmark protocol

Compare B0 (sequential full), B1 (parallel full), B2 (cache), B3 (correct fixed cadence), and P (freshness-aware policy). Same event traces, engine set, input cuts, warm-up policy and declared platform/budget for corresponding comparisons. Record policy-specific compute cost as part of results. Separate recorded-market traces from synthetic workloads and injected failure/burst traces.

Collect at least:
- Local event-to-output p50/p95/p99; separately record source-to-arrival/network latency when available.
- Throughput and sustainable rate; CPU utilization/CPU seconds, memory high-water mark and steady-state slope.
- Queue length, backlog, rejected/coalesced/dropped events; scheduler overhead.
- Per-engine source/result age distribution; freshness deadline miss fraction; unavailable/failed fractions.
- Cache hits/misses/invalidations and any mismatching reuse.
- Difference from B0 at **matched input cut and evaluation time**: exact/status agreement for executed fresh nodes; numerical deviation and decision disagreement for intentionally deferred nodes.
- Replay count/completeness and normalized output agreement; time to recover from injected faults.
- Ablations: minus caching, parallelism, cadence, cost estimator, and dependency-aware priority where interpretable.

For the primary comparative report, predefine workload profiles and repetitions (initial design target: at least 10 measured runs per configuration, with warm-up excluded and order randomized or alternated). Report individual run data, median and interval/spread; disclose machine, CPU governor/power state where measurable, OS, concurrency, runtime, data hashes and exact commands. Reconcile cold-start versus warmed execution separately.

No specific speedup or freshness-rate improvement is required for an honest experimental result. Any claim of superiority must be supported by the raw measured data, baseline fairness and uncertainty report.

## Release / paper gate

A research-ready release requires: source code and dependency lock; documented rights/availability for traces (or reproducible generators/acquisition scripts); event/schema versions; benchmark manifest and runner; raw results; figure-generation scripts; verified engine inventory; limitations and reproducibility instructions; appropriately preserved authorship; and a manuscript whose numerical statements can be traced to those artifacts.

The first migration/benchmark phase must not rely on unreliable legacy Sharpe/backtest estimates or the legacy documentation's outdated engine/test counts.
