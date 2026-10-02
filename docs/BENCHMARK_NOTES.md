# Local diagnostic benchmark — 2 October 2026

**Purpose:** build/test smoke comparison, not an independently repeatable paper result. Workload generated from deterministic simulated XAU/XAG bars and synthetic book snapshots; no live or historically recorded financial data were used. All source events carry `SIMULATED` mode.

Environment: Linux x86_64 kernel 6.18.44 container; reported CPU model AMD EPYC 9V74 80-Core; GCC 14.2.0, CMake 3.31.6; Release build. The shared/container CPU load, frequency control, thermal state and pinning were not controlled.

Method: ten successive runs for each policy, 300 sequential timestamps, 3 events per timestamp ingested, then **two** evaluations at the same input cut. B0 runs uncached full recomputation for both evaluations; B2 runs cached execution with the second evaluation permitted to reuse compatible outputs. This inherently favors cache reuse and is an intended smoke stress, not a fair scheduler comparison. Timings include source ingestion, engine execution and benchmark fixture generation. No I/O or network timing, affinity, randomized order, confidence intervals or full per-event resource traces; therefore **do not generalize speedup claims**.

Latest diagnostic (after implementing declared source allowlist; see `benchmarks/local_smoke_2026-10-02.txt`):

- B0 sequential full: median 8.934 ms over 10 runs (300 cuts x 2 evaluations), min 8.617 ms, max 9.606 ms; 64400 compute invocations and 0 cache hits summed across runs.
- B2 per-session cache: median 7.476 ms over 10 runs (300 cuts x 2 evaluations), min 7.335 ms, max 8.898 ms; 33650 compute invocations and 30750 cache hits summed across runs.
- Diagnostic numeric sink = 1372.329 for both policies (rounded by presentation); correctness tests additionally compare normalized outputs exactly at shared input cuts.

Acceptance evidence: Release-mode `metalarch_tests` passed 1,241 assertions spanning five test groups. Rebuilt and rerun AddressSanitizer and UndefinedBehaviorSanitizer (Debug) after the core read-allowlist and provenance changes. These tests are a starter suite, not a full correctness proof.

Recorded-input simulator (`fixtures/make_fixture.py`) generates `fixtures/synthetic_70.ma1`; two independent CLI replays should yield identical normalized text output with SHA-256 equality. See command examples in README. This is a fixture replay, not proof that a live market capture can be replayed faithfully.

Remaining evaluation: B1 parallel, B3 fixed cadence, P freshness scheduler, sufficient observed trace access, protocol-aware event handling, controlled CPU and memory profiling, per-event latency and deadline distributions, randomized benchmark order, repeatable raw result export and source license review.

## Follow-up five-policy synthetic diagnostic (2 October 2026)

The earlier 1,241-assertion/5-group note above is an **historical first-pass result**. Phase 2 includes 8 groups and a separate three-target CTest suite. B1, B3 and P are now implemented (see `IMPLEMENTATION_PHASE2.md`), but **P's nominal service-cost estimates are not calibrated**. New five-policy diagnostic: 300 bars x three simulated streams (900 accepted source events), 65-bar warm-up, ten repeated runs per policy in alternating order, 7,050 measured event outputs per policy. Local ingestion and execution only; no network, CPU pinning, stable-clock guarantee or empirical market data.

| Policy | p50 wall µs | p95 wall µs | B0 status differences | computations | explicitly deferred |
|---|---:|---:|---:|---:|---:|
| B0 | 22.35 | 26.40 | 0 | 68,150 | 0 |
| B1 | 24.15 | 33.39 | 0 | 68,150 | 0 |
| B2 | 17.36 | 26.79 | 0 | 28,200 | 0 |
| B3 | 17.76 | 26.25 | 3,540 | 27,610 | 3,540 |
| P | 21.98 | 27.78 | 4,700 | 28,200 | 4,700 |

Trace SHA-256: `d171a78b429ea06d6bd488e01bd62e8bb49bfc686ef4668d47dad05fd0978f1d`. Raw CSV SHA-256: `139838863e414738f507cf6e109d3d1a2d7c2e05f39804337601bd01636f3635` for this local run; run metadata and exact source hashes are in `artifacts/diagnostic_300_10/manifest.json` in the accompanying local artifact package. Differences for B3/P are explicitly disclosed stale statuses, not undetected cache corruption. B1's granularity optimization prevents the catastrophic thread-per-node overhead of the first prototype, but did not outperform B0 in the final diagnostic. The P run saved **no attempted computations compared with B2** under this one workload/budget, despite causing additional deferred statuses. Report this negative result and do not extrapolate to other workloads or claim scheduling advantage.

### Exploratory modeled-budget sweep (three repetitions per setting)

The sensitivity script (`benchmarks/run_budget_sweep.py`) tested nominal budgets 4, 6, 8, 10, 12 and 16 million against exactly the same 300-bar SIMULATED trace (first 65 bars excluded). At 4 million nominal units, P attempted 7,050 computations vs B2's 8,460 across three runs, while delivering 11,985 valid output slots versus B2's 20,445. At 8 million, P attempted 8,460 computations (the same as B2) but explicitly deferred 1,410 slots. At 16 million, P matched B2's 20,445 valid slots with no deferrals. **No general scheduling improvement is established**, and the nominal budget is not a measured real-time CPU constraint. Full sensitivity summary and raw files are distributed in the diagnostic archive; the script regenerates them.
