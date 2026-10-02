# Native execution policies — phase 2 (implemented 2 October 2026)

## What is implemented

B0: sequential, uncached full recomputation; reference output. B1: actual dependency-wave execution with a persistent session worker pool. The executor measures per-node observed cost and serializes cheaper waves to avoid thread launch/synchronization losses; cost measurements only affect B1's *performance scheduling*, not output values. B2: exact source/version/parameter/upstream cache lookup with independent source-age and result-compute-age checks. B3: fixed engine cadence with current-input identity check, bootstrap until a valid prior result exists, explicit STALE last-known metadata, no invalid value passed as VALID. P: dependency-aware, deterministic ready queue sorted by deadline slack / critical-path declared-cost estimate, with a per-evaluation nominal budget and cache hits excluded from compute charges. This is a preliminary scheduling policy, not an established novel algorithm.

Every policy uses one guarded stage evaluator and the same Store, Descriptor, Result and Graph contracts. All source and parent provenance is propagated even on downstream UNAVAILABLE results. A deferred prior value is stored only in `last_known_value`, `last_known_identity`, `last_known_source_ns`, and `last_known_computed_ns`, not in `value`. `RunStats` exposes cache hits, attempted computations, explicit deferrals and nominal estimated cost. A session requires serialized ingest/evaluation and monotonic logical evaluation times; it cannot be concurrently mutated by a streaming adapter.

**Design limitation:** P's `estimated_cost_ns` numbers are nominal fixed priorities, not measured or probabilistically calibrated runtime estimates. Declared budget is therefore a scheduling model, not a hard real-time CPU-time guarantee. No durable queue/backpressure, external adapter, recorded observational data or live deadline enforcement is claimed. B1 is allowed to be serial on tiny workloads: eager parallelism is counterproductive at microsecond-scale task costs.

## Independent checks

Eight test groups (4,691 local assertions in the final Release run) include dependency cycles, invalid descriptors, source/event validation and ingestion inversion, future-data causal access, source mutation and isolation, result age expiry, B0/B1/B2 differential correctness over many input cuts, true independent-thread overlap under a deliberately heavy fixture, cadence deferral and parent block, P urgent dependency closure and bounded nominal compute charges, B3/P replay through independent sessions, and parallel failure propagation. CLI and comparative-benchmark smoke tests are part of CTest. Run GCC Release, GCC ASan/UBSan and Clang Release; GitHub CI additionally targets macOS AppleClang (its outcome must be checked separately).

## Reproducible diagnostic

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 benchmarks/run_study.py --bars 300 --runs 10 --warmup-bars 65 \
  --budget-ns 8000000 --outdir artifacts/diagnostic_300_10
```

Script produces SHA-256 source, trace and binary manifest, raw per-event CSV, per-run output, and derived JSON summary. Initial 300-bar synthetic run reported B0/B1/B2 reference agreement and no unflagged source-age violation; the raw data and limitations are summarized in `docs/BENCHMARK_NOTES.md`. The generated `artifacts/` directory is intentionally gitignored because it can contain large experimental data and machine-specific paths. Source fixture generator is deterministic, all generated event modes are SIMULATED. An observed dataset and rights/semantics review are still required for a publication-ready systems study.
