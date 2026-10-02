# MetalArch M3 pilot — Opt-in shared rolling OHLCV feature index

**Date:** 2 October 2026. **Status:** An implemented, tested *subset* of M3, not a claim that all proposed incremental computation, subexpression deduplication, or production performance work is complete. The earlier ASEP2 manuscript, existing MetalArch B0 reference, and all MA1/MA2 fixtures are retained.

## Motivation and change boundary

The 19-stage native graph includes repeated computations of the same logarithmic price terms across four analytical stages. A new `Recent20` source-owned index updates a fixed 20-slot rolling window on **accepted** OHLCV bars only; exact duplicates, invalid bars, or unrelated book/peer sources cannot mutate that primary-bar index. Only descriptors that declare `requires_recent20=true` opt into it. Existing graph construction remains the reference with `use_incremental=false` by default.

- `src/core.cpp`, `include/metalarch/core.hpp`: shared per-source terms for `log(high/low)^2`, Garman–Klass per-bar contribution, squared log return, and absolute log return divided by `close * base_volume`. Terms are computed once at ingestion, with an O(1) 20-entry ring and rolling sums/counters; baseline sources allocate no index. Current `sizeof(Recent20)` is **880 bytes** on the tested Linux ABI, plus the optional pointer/allocation overhead; this is not a measurement of total process RSS.
- `src/engines.cpp`: `volatility` uses the recent return-square sum when its full valid 20-return window is available.
- `src/legacy_extensions.cpp`: `parkinson20`, `garman_klass20` and `amihud20` use the same indexed primary-bar terms. Original direct recomputation remains an explicit fallback if a window is incomplete or the indexed arithmetic is non-finite. The zero-volume Amihud result remains `UNAVAILABLE` rather than becoming zero or reusing another denominator.
- Every accelerated engine has a distinct `revision` and `parameters` namespace for correct versioned caching; its values are **not guaranteed bitwise-identical** due to non-associative floating-point addition/subtraction. No other 15 nodes have been renamed or advertised as optimized.
- `Session::reset()` clears stream-local measurements and cached terms but keeps the static declared opt-in. Late index activation rebuilds from retained source observations. Source version/digest changes only on accepted events; no concurrent ingestion is supported while B1 executes.
- CLI and benchmark `cohort` additionally accept `core-inc` and `expanded-inc`; `core`/`expanded` are unchanged. Graph/feature index initialization is performed by Session before any source is ingested.

## Tests and controlled comparison

The new `tests/test_incremental.cpp` completed **240,629 assertions across four groups**: engine descriptor isolation, duplicate/no-op and invalid-source handling; 5,200 synthetic bars covering warmup, flat bars, zero volume, primary ring wraparound at 4,096 and source isolation; late opt-in with the minimum 65-event Store; and all five policies with both 11/19-engine cohorts on the same 210-event MA1 trace.

Maximum absolute common-valid numerical error between original direct recomputation and opt-in rolling-window calculation on the tested fixtures: **3.77476 × 10^-15**; matching validity, mode, reason and source timestamp checks passed. Engine identities are deliberately different when the implementation revision differs. Independently, all **10 original, non-opt-in replay combinations** (`core`/`expanded` × `B0/B1/B2/B3/P`) were **byte-identical** between the prior M2 binary and M3, preventing silent alteration of the earlier manuscript reference.

All **18 CTest targets** passed under GCC Release, Clang Release, and GCC AddressSanitizer/UndefinedBehaviorSanitizer. Source-based local tests plus old suite comprise 248,018 reported assertions; this is not an independent estimate of full production code coverage.

### Synthetic pilot: same 300-bar trace, five runs per cohort

Events are **SIMULATED**; both studies share input trace SHA-256 `d171a78b429ea06d6bd488e01bd62e8bb49bfc686ef4668d47dad05fd0978f1d`. Each measurement includes in-process synthetic ingestion and full 19-stage graph execution after the same 65-bar warm-up. The two studies were run sequentially on an uncontrolled shared host: no randomized interleaving, CPU affinity, frequency/thermal control or confidence interval.

| Policy | Existing expanded p50 (µs) | Opt-in `expanded-inc` p50 (µs) | Difference (inc − ref, µs) |
|---|---:|---:|---:|
| B0 | 23.956 | 23.605 | −0.351 |
| B1 | 26.358 | 25.948 | −0.410 |
| B2 | 18.067 | 18.157 | +0.090 |
| B3 | 18.517 | 18.497 | −0.020 |
| P | 24.416 | 24.566 | +0.150 |

Both cohorts retained equal per-policy computation counts and explicit deferrals in the pilot. Full reference/cached baseline policies showed zero internal numerical/status disagreement within each cohort. **The acceleration did not reliably improve end-to-end latency**; in particular it slightly increased the B2 median in this local run. Up-front per-accepted-bar feature-indexing overhead and the relatively cheap existing 20-bar computations are likely contributors, but controlled ablations would be required to establish causality. Do not use this as proof of a publication-worthy throughput advantage. No improvement to the P scheduler is asserted.

## Reproduction

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/metalarch_cli inventory expanded-inc
./build/metalarch_cli replay fixtures/synthetic_70.ma1 b2 8000000 expanded-inc > /tmp/m3_incremental.txt
python3 fixtures/make_fixture.py --bars 300 --output /tmp/m3_300.ma1
python3 benchmarks/run_study.py --trace /tmp/m3_300.ma1 --cohort expanded --runs 5 --warmup-bars 65 --exe build/metalarch_compare --outdir /tmp/m3_baseline
python3 benchmarks/run_study.py --trace /tmp/m3_300.ma1 --cohort expanded-inc --runs 5 --warmup-bars 65 --exe build/metalarch_compare --outdir /tmp/m3_incremental
python3 benchmarks/check_m3_pilot.py /tmp/m3_baseline /tmp/m3_incremental --output /tmp/m3_pilot.json
```

The compact pilot report is `benchmarks/observations/2026-10-02_m3_rolling20_pilot.json`; the accompanying downloadable source/results package includes both raw event-level CSVs and full manifests. Independently rerunning on other machines can yield different timing results.

## Further M3 work — not yet implemented

1. Demand-based term masks (avoid computing GK/Parkinson/Amihud terms if only a volatility engine is registered), a shared cross-engine feature query API, and numerical drift thresholds/periodic rebase of incremental accumulators.
2. A controlled, interleaved heavy-workload study with measured per-engine wall/thread CPU, actual process memory and source-ingestion cost; demonstrate a meaningful benefit or retain the direct-compute implementation.
3. General incremental support for arbitrary windows and instrument/timeframe keys, selective invalidation after explicit source corrections, immutable snapshots for concurrent ingestion, and robust bounds under high source cardinality.
4. Full observed-source licensing/semantics review and CPU/RAM quotas remain outside this M3 pilot. The 100× roadmap is unchanged.
