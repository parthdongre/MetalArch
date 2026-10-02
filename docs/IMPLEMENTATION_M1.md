# MetalArch M1 — Opt-in Resource Instrumentation and Frozen Cost Calibration

**Status:** completed engineering milestone / synthetic diagnostic, 2 October 2026. **Not** a validated real-time metals workload, proof of scheduler advantage, hard CPU/memory budget, or finished publication result.

**Prior reproducibility anchor:** Phase III native graph at MetalArch commit `b80abd1268d3aa125958a0aa5eb6b9ed135d42e8`. Preserve all ASEP2 original manuscripts separately.

## M1 source changes and boundaries

1. Added `include/metalarch/resource.hpp` and `src/resource.cpp` for opt-in resource sampling. `CycleResources` includes per-cycle **process CPU**, current process RSS and process **lifetime** peak RSS. Linux current RSS uses `/proc/self/statm` and peak uses `/proc/self/status` **VmHWM**: Linux `getrusage().ru_maxrss` may inherit a Python launcher's high-water mark after fork/exec and was observed to falsely report ~95 MB for a ~2.4 MB native process. macOS uses `task_info` current RSS and `getrusage` peak RSS. Unsupported clocks return `nullopt` rather than fabricated CPU values. These are whole-process observations, not per-engine byte allocations.
2. `EngineMeasurement` optionally exposes the **compute-call-only** wall time, calling-thread CPU time (where available), engine ID and validity. B1 samples the actual worker thread. A non-profiled invocation does not perform per-engine thread CPU or RSS sampling. `metalarch_compare ... 1` writes both the event CSV and a `.stages.csv` per-computed-engine sidecar; `run_study.py` emits `engine_profile.json`.
3. A persistent-worker **pending-task queue high-water** is reported for B1. It is not an external source-ingest queue metric.
4. `EventWindow` now **lazily reserves** 64 slots initially (or fewer for a smaller maximum) and grows geometrically up to the configured maximum. At 220 bars per each of three fixture streams, the recorded contiguous `Event` reservation is 135,168 bytes (three × 256 × 176), versus 2,162,688 bytes previously reserved immediately (three × 4096 × 176). This is a **16× reduction in reserved contiguous event-slot capacity**, not a 16× resident-memory saving: in a five-pair `time -v` smoke comparison the native process RSS was similar between implementations, as the earlier reserve was mostly untouched virtual capacity. Strings, maps, metadata, allocator overhead, etc. are excluded from this structural capacity count.
5. Added `metalarch_calibrate`: runs **uncached B0 on a previous synthetic input interval**, excludes a warm-up window, collects at least five *valid* samples for every registered stage and exports per-stage median and p95 compute wall time plus thread-CPU p95 if supported. `cost_ns=max(1000, p95_wall + floor(p95_wall/5))` adds a 20% admission guard, **not a physical CPU quota**. A stable descriptor digest covers ID, revision, parameters, source identities/types, required parent IDs, freshness limits and cadence. The table loader fails closed for missing, extra, duplicate, zero, malformed or stale descriptor rows.
6. `CostModel::FrozenCalibration` is **opt-in only** for P, using the complete external table. The selection/critical-path costs and accounting use those frozen numbers throughout evaluation; profiler feedback from the held-out run cannot change admission decisions. The default P behavior remains the original declared-cost scheduler. `Session::reset` keeps the explicitly frozen table, preserving replay setup. `normalised_result` deliberately excludes profiling telemetry.
7. `metalarch_profile_session` runs one policy in its own lightweight native process *without retaining the comparison harness's B0 reference history*. Isolated memory observations are not contaminated by the comparison harness. All original five policies work with both 11-stage and 19-stage cohorts.
8. Synthetic fixture generation supports `--start-index`, permitting non-overlapping calibration and held-out evaluation intervals. `benchmarks/run_m1_study.py` records the two trace SHA-256 hashes, complete cost-table SHA-256, source manifest, OS and cgroup readings, normal vs profiled timing runs, and isolated-session RSS. CPU and RSS are sampled, **not enforced** by the scheduler or benchmark.

## Correctness and local testing

- `tests/test_resource.cpp` tests the supported/unsupported resource-sampler contracts, lazily bounded EventWindow behavior, optional telemetry and B1 worker profiles, missing and mismatched frozen calibration rows, invalid costs, and the explicit cost-based P admission behavior.
- 10/10 CTest targets passed under GCC Release, Clang Release and GCC Debug ASan/UBSan. The existing 4,723 native assertions, 1,668 ASEP2-derived extension assertions and the additional 63 dedicated M1 assertions passed (6,454 total). This count excludes extra CTest CLI/replay assertions.
- A differential replay comparison between the previous Phase III code and this M1 code found **byte-identical normalised outputs across all ten combinations** of core/expanded cohorts × B0/B1/B2/B3/P on one independent 220-bar input. Instrumentation never enters `Result::identity` or numerical computation.
- Remote macOS CI should be verified separately against the pushed commit; local Clang on Linux does not establish AppleClang compatibility.

## Five-run M1 synthetic pilot (NOT observed market data)

- Calibration: 150 simulated bars (indices 1–150), exclude first 65 bars = 255 post-warm-up evaluation events. Calibration is B0 only, independent of subsequent evaluation.
- Held-out evaluation: 220 simulated bars (indices 2001–2220), exclude 65 warm-up bars = 465 measured input events per run, **five runs/policy** = 2,325 measured events/policy. Three sources: fixture XAU 1m, fixture XAG 1m, fixture XAU top-book.
- Calibration trace SHA-256: `24e4357f66061ae2788222200c7d47b4d6d94f48a0b407a3f3ccbe153b7e4c90`.
- Held-out trace SHA-256: `2b843c506b5255026962b75db2a6433fcf48edae547c1d3ae7fa9de91967180b`.
- **Declared P** cost budget: 8,000,000 descriptor units per evaluation.
- **Frozen P** cost table from earlier interval: summed per-engine frozen cost 23,051 ns-like units; pilot budget `floor(.65 × sum)=14,983` units. These are intentionally different calibration and declared budget scales; **P's different deferral counts are NOT evidence of a fair same-budget win**.

| Study (no profiler) | B0 p50 µs | B1 p50 µs | B2 p50 µs | B3 p50 µs | P p50 µs | B2 computations | P computations | P explicit deferrals |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Declared-cost P comparison | 24.286 | 26.649 | 18.287 | 18.768 | 25.027 | 15,500 | 14,725 | 13,950 |
| Frozen-cost P comparison | 23.114 | 25.518 | 17.636 | 18.087 | 24.757 | 15,500 | 15,500 | 3,875 |

B0/B1/B2 all produced **zero status disagreements** against B0 at matched input cuts in both benchmark conditions. B3/P status differences are explicit deferrals. An important negative result: at this **frozen calibrated budget**, P performs exactly as many actual compute calls as B2 (15,500) but yields 3,875 additional deferred result slots; its broader admission/priority mechanism needs further revision. B1 is slower on this cheap workload. No measured superior scheduling performance is claimed.

**Profiler overhead must be reported**: in the declared-cost comparison, B0 p50 increased from 24.286 to 42.283 µs, and B2 from 18.287 to 31.507 µs when process/thread sampling was enabled. Do not mix or subtract profiled and unprofiled results as if independently controlled or experimentally free. These trials ran sequentially on a shared host without pinned cores/thermal control, so these are implementation diagnostics, not publication-grade effect sizes.

### Isolated native session memory (660 input events including warm-up)

| Policy | Peak RSS bytes | Native source-event reserved-slot capacity bytes |
|---|---:|---:|
| B0 | 2,441,216 | 135,168 |
| B1 | 2,600,960 | 135,168 |
| B2 | 2,445,312 | 135,168 |
| B3 | 2,445,312 | 135,168 |
| Frozen P | 2,453,504 | 135,168 |

These are separate native subprocesses; even they include the executable, libraries, heap, metadata and source store, and are not per-engine allocation measurements. External Linux `/usr/bin/time -v` repeated five-pair smoke showed similar old/new maximum RSS despite the substantially smaller contiguous vector reservation. Hardware and source rights still limit generalisation.

## Reproduction

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# Prior only (do not calibrate on held-out evaluation events):
python3 fixtures/make_fixture.py --bars 150 --start-index 1 --output artifacts/calibration.ma1
./build/metalarch_calibrate artifacts/calibration.ma1 artifacts/frozen_costs.tsv expanded 195 5
# CLI replay with an explicit frozen table:
./build/metalarch_cli replay fixtures/synthetic_70.ma1 p 14983 expanded artifacts/frozen_costs.tsv > outputs.txt

# Full disjoint-trace five-run synthetic pilot and isolated memory probes:
python3 benchmarks/run_m1_study.py --build-dir build --calibration-bars 150 \
  --evaluation-bars 220 --evaluation-start-index 2001 --runs 5 \
  --outdir artifacts/m1/research_pilot
```

The pilot artifact contains immutable generated MA1 calibration/evaluation traces, `frozen_costs.tsv`, four policy comparison CSV and manifest sets, two large per-computed-engine profile CSVs and summaries, isolated process CSVs, and the aggregate `m1_report.json`. Do not re-label synthetic XAU/XAG values as observed prices.

## Remaining milestones (not implemented in this PR)

- Full per-engine byte allocation counts would require a reliable allocator instrumentation strategy; RSS and reserved contiguous Event slots are different quantities.
- Physical enforceable CPU quotas/memory admission and source backpressure (M3–M4), rather than nominal scheduler budget accounting.
- Legal observed-market source recording and independent traces (M2).
- Calibrated cross-platform observed workloads, controlled effect sizes and externally validated scheduler novelty (M5).
- The full ASEP2 terminal UI and all historical engines are **not** yet ported; this is still the selected 19-stage mathematical workload.
