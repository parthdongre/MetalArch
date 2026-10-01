# MetalArch

**Research prototype (C++20).** An independently implemented analytical execution core using synthetic precious-metal workloads. This is **not** the original ASEP2 terminal, a validated prediction model, a production market feed, a verified patent, or a completed research paper.

## Implemented in this snapshot

- Standard-library C++20 core with strict DAG validation (missing dependencies, cycles, duplicate identifiers); ready nodes are executed deterministically in topological order. **No parallel executor is claimed yet.**
- Enforced per-engine source read declarations; downstream engines receive only their declared upstream results.
- Bounded per-stream histories (default 4096 records), version and rolling source-content digests, idempotent latest-sequence duplicates, explicit rejection of divergent duplicates, invalid and out-of-order events.
- Per-session state and last-valid-result cache keys using engine ID, revision, parameters, source key/version/digest, declared upstream identities/status and declared age policy. Cache reuse checks age; stale/unavailable/failed results are never converted to valid neutral scores.
- Input provenance modes (observed, estimated, simulated), source/event timestamps, execution-session clock inputs, source lineage, explicit result states.
- Versioned text event codec (`MA1`) and replay CLI using the same execution code path as normal sessions. Reproducibility is currently verified on the **same machine/build**; distributed or cross-platform bitwise determinism is not claimed.
- Eleven audited-by-implementation workload stages: EMA(8,21) trend, RSI(14), RMS log-return volatility(20), ATR(14) simple average, timestamp-matched Pearson peer correlation, top-of-book imbalance, 64-observation Goertzel spectral concentration (eight bins), and deterministic illustrative regime/risk/forecast/fusion. These final four are heuristic engineering fixtures, **not empirical financial models**.
- Release-mode tests, optional ASan/UBSan, standalone B0 sequential-full versus B2 cache smoke benchmark.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/metalarch_tests
./build/metalarch_bench

# On a supported Clang/GCC toolchain:
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DMETALARCH_SANITIZE=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure

# Purely synthetic, deterministic trace:
python3 fixtures/make_fixture.py
./build/metalarch_cli replay fixtures/synthetic_70.ma1 > replay.txt
```

CMake >=3.20 and an available C++20 compiler are required. No external C++ dependencies. Python is needed only to regenerate the optional deterministic fixture, not for runtime analytics. Linux/GCC 14 test results are available; **macOS/Apple Silicon is not yet independently verified.**

## Trace schema

`MA1|B or L|source|symbol|timeframe|sequence|event_ns|ingest_ns|a|b|c|d|e|mode`

`B`: a=open, b=high, c=low, d=close, e=volume. `L`: a=bid, b=ask, c=bid quantity, d=ask quantity, e=reserved. Mode is 0 observed / 1 estimated / 2 simulated. Fields are pipe-separated; source keys cannot contain pipes/newlines. Malformed, out-of-order and conflicting duplicate records are rejected. The rolling FNV-style digest is a fast non-cryptographic **cache identity aid**, not an authenticity mechanism; use an external SHA-256 manifest for trace integrity. Latency measurements must distinguish event and ingestion time. The current schema does not claim lossless capture of a live exchange depth feed.

## Research and migration integrity

Original ASEP2 is separately preserved; none of its unreviewed components, datasets or publication metrics is automatically imported. Historical paper author list: Ramakrishna Bharsakde, Parth Dongre, Parth Birari, Atharv Patil, Aryan Patil (confirm revisions and contributor agreements before publication). The old ASEP2 paper's engine counts and tests must not be advertised as MetalArch outcomes. Synthetic benchmark timings here are preliminary diagnostics, **not** a paper-ready comparison.

See `docs/ENGINE_AUDIT.md`, `docs/BENCHMARK_NOTES.md`, `docs/PAPER_SCAFFOLD.md` and the repository's prior planning files for the future research work. In particular, B1 parallel, B3 cadence, and freshness scheduling policy P, immutable append-only trace storage, external macro feed validation, artifact locking and full evaluation are still future work.

**Patent/publication caution:** Public disclosure may impair patent eligibility in some jurisdictions. This repository contains conventional reference-system work, not a vetted patent claim. Obtain qualified advice and conduct prior-art searches before publicly publishing any genuinely new confidential scheduling mechanism.