# Phase III — Audited ASEP2 workload expansion and cohort-scaled diagnostics

**Date:** 2026-10-02. This is a reproducible local synthetic test report, **not** publication-grade throughput or a patentability/novelty finding. Historic ASEP2 remains unchanged.

## Implemented software changes

- `src/legacy_extensions.cpp` and `include/metalarch/legacy_extensions.hpp` add eight independent native C++20 implementations adapted from the actual original ASEP2 code (`oscillator_bank.py`, `realized_vol.py`, `amihud_roll.py`, `volume.py`, `changepoint.py`). See `ENGINE_AUDIT.md` for units, formulas, warm-up lengths, and semantic differences from historical results.
- `make_metal_graph(..., include_legacy=true)` exposes an **expanded 19-stage workload**; `include_legacy=false` preserves the previous **11-stage core**. There is no pretending that all historical ASEP2 registered engines have been ported.
- `metalarch_cli inventory core|expanded` emits deterministic tab-separated metadata: ID, implementation revision, parameter string, required sources/parents, TTLs, cadence, and nominal cost. It generates engine counts from executable descriptors rather than manually copied paper figures.
- Both cohorts use exactly the same reference, parallel, cache, cadence, and preliminary freshness scheduler. Benchmark commands accept `--cohort core|expanded`, and the manifest records the selected stage count and source-file hashes.
- Fixed the optional plot script's previous **hard-coded 11-stage denominator**. It now derives the stage count from the adjoining manifest (or requires `--engine-count`), preventing inaccurate deferral fractions for expanded workloads.
- Edge cases from the old algorithms have explicit validity rather than a neutral score: zero-range Williams %R, zero-MAD CCI, and positive-volume requirements for Amihud. The volume-adapted OBV surge is zero if the latest volume is zero. No fixed annualization is applied to OHLC volatility estimates without independently established bar semantics.

## Verification

- `metalarch_tests`: **4,723 assertions** across eight existing core groups, now using the 19-stage default workload.
- `metalarch_legacy_tests`: **1,668 assertions** across four new groups, including minimum input lengths, independent closed-form numerical oracles, zero-range/flat/zero-volume cases, declared-source access, B0/B1/B2 differential agreement and cache invalidation/isolation.
- All **six CTest targets** passed in GCC Release, GCC ASan/UBSan Debug, and Clang Release locally. CTest covers replay and policy smoke testing for **both** cohorts.
- New numerical values are *features*, not financial returns, tradable predictions, estimated probabilities or the original terminal's normalized recommendation scores.

## Phase III three-repeat paired diagnostic

The two cohorts use the same deterministic 300-bar, three-stream synthetic input trace, with 65 warm-up bars removed, three runs per policy, and identical nominal P budget **8,000,000 declared-cost units**. Trace SHA-256: `d171a78b429ea06d6bd488e01bd62e8bb49bfc686ef4668d47dad05fd0978f1d`. Timings include **local input ingestion + execution**, not network transit. Measurements were taken on one shared host without power/thermal control, CPU affinity or enforced memory/CPU quotas. Do not interpret lower microsecond numbers from this short pilot as a population-level improvement.

| Cohort | Policy | p50 (us) | p95 (us) | Computed stage invocations, 3 runs | Current valid output slots | Explicit deferrals |
|---|---|---:|---:|---:|---:|---:|
| core 11 | B0 | 17.93 | 22.75 | 20,445 | 20,445 | 0 |
| core 11 | B1 | 19.30 | 22.76 | 20,445 | 20,445 | 0 |
| core 11 | B2 | 13.73 | 20.89 | 8,460 | 20,445 | 0 |
| core 11 | B3 | 14.05 | 20.11 | 8,283 | 19,383 | 1,062 |
| core 11 | P | 17.41 | 21.27 | 8,460 | 19,035 | 1,410 |
| expanded 19 | B0 | 24.32 | 28.19 | 37,365 | 37,365 | 0 |
| expanded 19 | B1 | 26.69 | 34.58 | 37,365 | 37,365 | 0 |
| expanded 19 | B2 | 18.54 | 28.88 | 14,100 | 37,365 | 0 |
| expanded 19 | B3 | 18.98 | 28.05 | 13,923 | 36,303 | 1,062 |
| expanded 19 | P | 25.02 | 29.58 | 13,395 | 24,675 | 12,690 |

B0/B1/B2 had **zero status or common-valid numerical disagreements** at matched event cuts and zero unflagged valid-source-age violations on this fixture. The B3/P status differences represent disclosed cadence/scheduler deferrals rather than misidentified current results. B1 still pays overhead on these lightweight computations. As the cohort expands, the fixed nominal 8-million budget defers many more P outputs—evidence that budget calibration and priority design are the next actual research bottleneck, not evidence of a win.

In a **separate exploratory two-run expanded-cohort sweep** on the same synthetic input, a P budget of 8 million produced 16,450 valid slots and 8,460 explicit deferrals; at 24 million, P delivered 24,910 valid slots with zero deferrals, matching B2's availability in that sweep. This is *nominal cost accounting*, not evidence that execution fits a corresponding nanosecond CPU deadline. Sweep records and raw CSV are included in the accompanying artifact, not required for ordinary builds.

Reproduce:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/metalarch_cli inventory expanded
python3 benchmarks/run_study.py --bars 300 --runs 3 --warmup-bars 65 --cohort core --budget-ns 8000000 --outdir artifacts/phase3_core
python3 benchmarks/run_study.py --bars 300 --runs 3 --warmup-bars 65 --cohort expanded --budget-ns 8000000 --outdir artifacts/phase3_expanded
python3 benchmarks/plot_results.py artifacts/phase3_expanded/events.csv --outdir artifacts/phase3_expanded/figures
```

The benchmark's timestamps, raw CSV files, binary SHA-256, input-trace SHA-256, source hash manifest, and output calculations accompany the artifact. New runs on different hardware will legitimately yield different wall-clock results. The prior **Phase II 11-stage ten-repeat** measurements remain in `BENCHMARK_NOTES.md` and the continuation manuscript and must not be retrospectively relabeled as 19-stage results.

## Unresolved scientific and engineering requirements

- **Measured resource caps:** P still uses descriptor-declared nominal costs, not measured/enforced CPU or memory budgets. It is especially weak on the expanded workload at the original 8M budget.
- **Source fidelity:** all diagnostic inputs here are SIMULATED; base-volume * price is not automatically verified quote-volume, and live source rights/semantics remain untested.
- **Cross-engine common subexpressions:** current features independently read bounded OHLCV windows; a future shared feature snapshot or incremental update path requires correctness testing against the uncached oracle and fair overhead accounting.
- **Scaling/stress:** vary actual CPU quotas, memory caps, bar frequency, worker counts, event bursts and heavy-stage cost distribution; repeat on controlled hardware, collect uncertainty, and study fallback/recovery under injected faults.
- **Paper status:** retain original ASEP2 authors and source material; paper claims must identify the source revision, cohort and actual experimental configuration. Patentability and novelty still require prior-art and jurisdictional review.
