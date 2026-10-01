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