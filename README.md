# MetalArch

**Research prototype: C++20 dependency-aware streaming analytics core.** It is a new implementation, not a rename of ASEP2, a production brokerage system, a validated forecasting model, an established patent, or a completed publication.

## Implemented execution modes

| Policy | Behaviour |
|---|---|
| B0 | Correct sequential uncached full recomputation — reference/oracle |
| B1 | Declared-DAG wave executor with persistent bounded worker pool; cheap waves execute serially to avoid excessive overhead |
| B2 | Per-session source-version/parent-version cache with independent input/result age contracts |
| B3 | Correct fixed cadence and separately disclosed last-known output when deferred |
| P | Preliminary deadline-slack, dependency-closure, declared-cost-budget ready queue; research value still unverified |

A typed `Descriptor` declares inputs, required parents, revision, parameters, source age, result age, cadence and nominal compute estimate. Missing dependencies, cycles and inconsistent descriptors fail early. Input keys include source, symbol, timeframe and kind. The ingestion store rejects non-finite/invalid events, sequence/time inversions, conflicting duplicates and invalid provenance modes, retains bounded history through an O(1)-overwrite contiguous ring (default 4,096 events/stream), and exposes versioned rolling-content identity. Engines can read only declared sources. Downstream failures and provenance propagate explicitly. The source fingerprint uses a fast **non-cryptographic** digest; the manifest separately uses SHA-256 for artifact integrity. Ingest and evaluation must be serialized within one Session.

**Workload selection:** the original 11-stage core remains accessible as `core`. The default `expanded` cohort contains **19 stages**: the core plus eight independently implemented ASEP2-derived C++ methods (ROC(12), Williams %R(14), CCI(20), Parkinson(20), Garman–Klass(20), Amihud(20), OBV/volume surge(30), and CUSUM(40)). These are audited *adaptations*, not an assertion that all legacy engines or their old combined normalized scores have been reproduced. The four inherited heuristic core nodes remain illustrative, not validated forecasts. Both cohorts use the same events, scheduler implementation and source/provenance contract. Source modes are OBSERVED/ESTIMATED/SIMULATED; generated fixture data are exclusively SIMULATED. See `docs/IMPLEMENTATION_PHASE3.md` and `docs/ENGINE_AUDIT.md` for definitions and edge cases.

## Build and verify

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/metalarch_tests
./build/metalarch_legacy_tests

# GCC/Clang sanitizer configuration
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DMETALARCH_SANITIZE=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

CTest covers the native assertions, a real trace replay and all five policy smoke executions. The implementation has also been tested with GCC Release, GCC ASan/UBSan, and Clang Release locally. The GitHub workflow separately tests Ubuntu with GCC/Clang and macOS AppleClang; check workflow results rather than assuming remote cross-platform success.

## Deterministic replay

MA1 schema: `MA1|B or L|source|symbol|timeframe|seq|event_ns|ingest_ns|a|b|c|d|e|mode`.
`B`: open/high/low/close/volume; `L`: bid/ask/bid_qty/ask_qty/reserved. Mode `0/1/2` is observed/estimated/simulated. This minimal schema does not reproduce full live exchange order-book delta semantics. Global ingestion timestamps must be non-decreasing. Source age and result computation age are distinct. Deferred outputs carry optional prior metadata but never pass its old value to a required downstream engine as VALID.

```bash
python3 fixtures/make_fixture.py --bars 70 --output fixtures/synthetic_70.ma1
./build/metalarch_cli replay fixtures/synthetic_70.ma1 b2 > cached.txt
./build/metalarch_cli replay fixtures/synthetic_70.ma1 b3 > cadence.txt
./build/metalarch_cli replay fixtures/synthetic_70.ma1 p 8000000 > scheduled.txt
```

CLI policies: `b0`, `b1`, `b2` (default), `b3`, `p`. A final optional `core|expanded` argument selects the registered cohort; `expanded` is the default. The CLI can also report an inspectable workload inventory with `./build/metalarch_cli inventory core` or `./build/metalarch_cli inventory expanded`. Replay uses the same execution implementations as interactive sessions with a controlled input clock, not a reconstruction of engine-score telemetry. Exact same-platform replay is tested; cross-platform bitwise agreement is not guaranteed.

## Cohort-scaled diagnostic (Phase III)

Compare the 11-stage and 19-stage cohorts on the same deterministic fixture:

```bash
python3 benchmarks/run_study.py --bars 300 --runs 3 --warmup-bars 65 \
  --cohort core --budget-ns 8000000 --outdir artifacts/core
python3 benchmarks/run_study.py --bars 300 --runs 3 --warmup-bars 65 \
  --cohort expanded --budget-ns 8000000 --outdir artifacts/expanded
python3 benchmarks/plot_results.py artifacts/expanded/events.csv --outdir artifacts/expanded/figures
```

The benchmark manifest includes cohort name, dynamically verified stage count, SHA-256 for every native source and trace, and exact commands. Never directly compare a policy's absolute latency on **different engine cohorts** as though their computational workloads are equal. See `docs/IMPLEMENTATION_PHASE3.md`. This study remains synthetic and does not enforce a real CPU/memory budget; P charges descriptor-declared nominal costs.

## Prior Phase II eleven-stage diagnostic benchmark

```bash
python3 benchmarks/run_study.py --bars 300 --runs 10 --warmup-bars 65 \
  --budget-ns 8000000 --outdir artifacts/diagnostic_300_10
```

Output: `events.csv`, `summary.json`, `manifest.json`, `per_run.txt` and a SHA-256-labelled synthetic trace. Benchmark accepts the documented *fixture* XAU/XAG bars and book, not arbitrary claimed observational feeds. Modeled cost budget is based on declared initial cost estimates rather than measured/guaranteed actual CPU runtime. The first extended diagnostic found B0/B1/B2 status and numerical agreement with zero unflagged source-age violation. B1 did not outperform B0 on this lightweight workload; P deferred additional outputs without reducing compute invocations versus B2 for the tested budget. See `docs/BENCHMARK_NOTES.md` and `docs/IMPLEMENTATION_PHASE2.md` for numbers and caveats. **These are not publication-ready financial-data or real-time scheduling results.**

## Paper continuity (ASEP2 → MetalArch)

The primary [`paper/METALARCH_DRAFT.md`](paper/METALARCH_DRAFT.md) directly extends the ASEP2 Metals Terminal paper. The historical originals are preserved verbatim in [`paper/legacy/IEEE_RESEARCH_PAPER.md`](paper/legacy/IEEE_RESEARCH_PAPER.md) and [LaTeX](paper/legacy/IEEE_RESEARCH_PAPER.tex), while the earlier standalone systems draft is retained in [`paper/archive/METALARCH_SYSTEMS_DRAFT_2026-10-02.md`](paper/archive/METALARCH_SYSTEMS_DRAFT_2026-10-02.md). `paper/CONTINUATION_MAP.md` distinguishes original work, native implementation, and planned publication evaluation.

## Research integrity and history

Original ASEP2 historical paper authors (original order, subject to contribution/affiliation confirmation for a revision): Ramakrishna Bharsakde, Parth Dongre, Parth Birari, Atharv Patil, Aryan Patil. Historical counts and claims belong to their exact prior revision. Do not silently reuse external data or original code without rights and provenance review. See `docs/LEGACY_AUDIT.md`, `paper/METALARCH_DRAFT.md`, and `docs/ACCEPTANCE.md`. Novelty, patent eligibility, predictive performance, observed-source coverage, and peer-reviewed publication remain **unverified**. Public disclosure can affect patent strategy; any confidential inventive claim requires qualified prior-art/filing review before publication.
