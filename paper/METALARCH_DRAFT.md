# MetalArch: Resource-Efficient Execution for Multi-Engine Precious-Metal Analytics

**Status:** Integrated continuation manuscript, 2 October 2026. Working academic draft—not a published paper or a claim of proven novelty, patentability, or production performance. Quantitative MetalArch findings below are limited to an initial **synthetic diagnostic**. Preserve the source manuscript without silently rewriting its historical claims.

**Historical ASEP2 author order (retain as provenance; confirm the revised paper's individual contributions and affiliations with all authors before submission):** Ramakrishna Bharsakde, Parth Dongre, Parth Birari, Atharv Patil, Aryan Patil.  
**Original affiliation:** Department of Engineering, Sciences and Humanities (DESH), Vishwakarma Institute of Technology, Pune, Maharashtra, India.  
**Project lineage:** F.Y.B.Tech ASEP2, semester 2 A.Y. 2025–26 (Metals Terminal, June 2026) → MetalArch software-systems extension (October 2026). The unmodified originals are retained under `paper/legacy/` in this repository.

## Abstract — provisional

Our previous ASEP2 project, *Metals Terminal*, developed a local multi-engine research environment for precious-metal analysis. Its scope included historical and streaming market-data interfaces, quantitative engines spanning several analytical families, result fusion, telemetry, a browser-based interface, and optional native acceleration. The original paper demonstrated an integrated research application and identified efficient operation on local undergraduate hardware as a non-functional requirement. Extending that work exposed a more specific problem: heterogeneous engines require different inputs and computation times, yet a resource-constrained machine must manage dependency ordering, recomputation, output age, and degraded sources without presenting reused or unavailable results as current evidence. This continuation paper describes MetalArch, an experimental C++20 analytical core that treats these properties as explicit execution contracts while retaining the previous terminal's precious-metal workload as its application foundation. The original native prototype contained eleven representative stages; a subsequent ASEP2-derived expansion now supports nineteen stages, with the original cohort retained for paired workload experiments. Five execution policies are available: sequential full recomputation (B0), dependency-wave parallel execution (B1), version-aware caching (B2), explicitly stale fixed cadence (B3), and a preliminary nominal-budget/deadline policy (P). A first ten-repeat synthetic diagnostic finds numerical and status agreement among B0/B1/B2 at matched input cuts, lower repeated computation under B2, and no demonstrated advantage for P over B2 at the chosen nominal budget. This is a foundation for a later controlled resource-budget and recorded-observation study, not a completed demonstration of scheduling superiority or predictive market quality.

**Keywords:** precious-metal research terminal; resource-constrained systems; heterogeneous analytics; dependency graph; selective recomputation; provenance; freshness; C++20; reproducible replay.

## I. Introduction: Continuation of the Previous Semester's Work

The original ASEP2 manuscript, *A Real-Time Multi-Engine Research Terminal for Precious-Metal Market Intelligence* (June 2026), addressed whether a local undergraduate engineering project could combine market ingestion, microstructure, technical indicators, statistical analysis, interpretable visualization, validation-related tooling, and desktop-oriented distribution without relying on external compute infrastructure. Its stated outcome was not automated trading or a proven forecasting model. Rather, it built an integrated research instrument in which independent analyses could be inspected and summarized. The original manuscript's stated data configuration centered on `XAUUSDT` and `XAGUSDT`; actual present-day exchange availability, data rights and instrument semantics have **not** been independently verified for MetalArch.

The present study **continues, rather than replaces**, that problem. The first version emphasized the breadth and integration of analysis; this version studies the cost and correctness of executing such analysis on bounded resources. Adding more indicator families does not by itself answer whether a multi-engine system can preserve reliable outputs when several inputs change at different rates, expensive nodes lag behind, and the execution budget is insufficient for full recomputation. Nor can a fast cached output be taken as correct merely because the terminal responds promptly. These observations lead to the continuation question:

> Under a fixed local resource budget, what computational work can be eliminated from the existing multi-engine analytical workflow, and what freshness or analytical availability is sacrificed, while maintaining explicit dependency and output-validity contracts?

**Scope:** This is a software-systems study with precious-metal analytics as its concrete inherited workload. Numerical market prediction, trading profitability, and claims of privileged data coverage are not used as systems-performance evidence.

## II. Phase I Foundation: Metals Terminal (ASEP2, June 2026)

This section preserves the main organization and substance of the earlier paper. Its implementation descriptions are **historical**, not claims that the same frontend or complete engine set has been migrated to the current MetalArch branch.

### A. Market analysis framework retained from ASEP2

Metals Terminal treated evidence from multiple families as complementary rather than allowing a single indicator to dominate. The original framework asked whether the market was trending, ranging, or reversing; whether momentum was supported by participation and order flow; whether volatility and tail-risk were changing; whether spreads and available liquidity supported the observed movement; and how conclusions changed under adverse market scenarios. Its engine families included trend/momentum, volatility/risk, microstructure/volume, regime/changepoints, spectral/complexity, cross-asset/systemic analysis, forecasting/macro/sentiment, and downstream signal fusion. **These families remain the application-level design and workload-selection basis for MetalArch.** A numeric score in the prior interface was an interpretative display convention, not proof of calibrated predictive probability.

### B. Earlier engineering architecture

The previous manuscript describes a Python `metals` package, configurable REST/WebSocket data sources, an engine registry, an orchestrator producing result bundles, snapshot serialization, a FastAPI/WebSocket backend, browser-based chart panels, local telemetry/storage, Monte Carlo controls, and optional C++ kernels. The original paper also discusses bounded tick storage, slower engine cadence and desktop packaging as ways of maintaining local usability. These were meaningful project deliverables and motivate the computational constraints investigated here. The historical manuscript and original source remain independently archived for reproducibility and attribution.

### C. Historical evidence boundary

The original June manuscript states **71 registered engines** and **38 passing automated tests** for its reported version. An independently supplied later local review records **92 passes and two warnings** in that reviewer environment; inspection of its imported registry finds **58 distinct registered engines** in that particular local snapshot. The connected ASEP2 GitHub `main` is another revision. These facts must not be combined into a single current count, and none is a MetalArch benchmark. Exact prior capabilities, native speedups and installer-signing statements should be reverified against a pinned legacy release before repeated as established measurements. The archived original, including its original wording, is retained at `paper/legacy/IEEE_RESEARCH_PAPER.md` and `.tex`; a separate legacy audit describes revision-specific discrepancies.

## III. Problem Identification from Phase I

The earlier paper's non-functional requirements explicitly included local operation, controlled memory, deferring expensive engines, a responsive interface, testability and graceful handling of missing external sources. Its section on runtime efficiency proposed capped thread counts and workers, bounded queues, and slow-engine reuse. The extension arises directly from these previous requirements, not from an unrelated scheduling problem.

A subsequent code audit of the provided legacy snapshot identified four relevant implementation limitations:

1. **Dependency ordering.** Registered `requires` relationships were not the source of the hand-assigned layer schedule. Several declared parents were assigned to the same or a later layer, so downstream computations could observe absent upstream results.
2. **Cache correctness and isolation.** A short fingerprint containing primary-frame length, latest timestamp and rounded latest close did not identify all relevant peer, book, window, parameter and parent changes. Global memoization/cadence could also reuse a context-incompatible result.
3. **Validity and freshness.** A missing or failed parent could be replaced by a default `EngineResult` with a neutral score. An apparently ordinary numerical output therefore did not always distinguish a valid neutral state from unavailable evidence.
4. **Reproducibility.** The later legacy replay reconstructed output-like bundles with empty frames rather than replaying original input events through the production dependency path. The original walk-forward/ML claims also need separate revalidation if predictive quality is discussed.

These are findings for the **inspected historical revision**, not an assertion that none of ASEP2's integration achievements existed. They motivate a principled execution-layer redesign while retaining the application and prior paper's engineering contribution.

### Formal resource-constrained execution model

Let the analytical workload be a directed acyclic graph \(G=(V,E)\), where node \(i\) has declared sources \(S_i\), parents \(\operatorname{pred}(i)\), parameters \(\theta_i\), a source-age limit, a result-age limit and an estimated cost \(\hat c_i\). A current computation is

\[
y_i(t)=f_i\left(X_{S_i}(t),\{y_j(t):j\in\operatorname{pred}(i)\};\theta_i\right).
\]

Input events have distinct event and ingestion timestamps. A node cannot consume a measurement that has not arrived by its logical evaluation cut, and an expired or failed required parent cannot silently supply a numerical value as fresh evidence. For any execution policy \(\pi\), the study records actual computations, local processing latency, output-validity distribution and age, CPU/memory, and disagreement from correct full recomputation at matched cuts. The evaluation should examine the trade-off under explicitly controlled CPU, memory and nominal or measured scheduling budgets, **without assuming a policy must win on every metric**.

## IV. Phase II Objectives and Research Questions

This continuation studies:

- **RQ1 — Dependency and cache correctness:** Can derived DAG execution and complete declared-input/parent identities preserve normalized results against an uncached oracle on equal input cuts?
- **RQ2 — Constrained execution:** When event arrival rate, compute budget or available workers are constrained, how do sequential full, parallel full, selective caching, fixed cadence and deadline-aware selection differ in real compute work, output age and availability?
- **RQ3 — Reproducibility:** Can recorded *inputs* reproduce the same normalized result/status sequence through the actual analytical path under a controlled logical clock?
- **RQ4 — Evidence integrity:** Can observed, estimated and simulated input lineage and explicit status prevent failed, missing or intentionally stale values from appearing as valid neutral evidence?

The intended incremental contribution is **not** the invention of EMA, RSI, a generic DAG, or ordinary caching. It is the implementation and reproducible evaluation of their interaction in the existing precious-metal terminal's heterogeneous, resource-bounded setting. The exact research gap and any stronger novelty claim remain subject to a sourced related-work review.

## V. MetalArch Extension Architecture and Implementation Status

The previous system can be understood as: **market adapters → engine orchestration → result bundle/fusion → FastAPI/WebSockets → browser/desktop interface**. The MetalArch extension targets the middle computational seam:

```text
ASEP2 application foundation (historical; production bridge pending)
  Data adapters / precious-metal analytical families / research UI
                           |
             versioned source-event boundary
                           v
MetalArch native execution core (implemented C++20 prototype)
  source validation + bounded event history + lineage
                           |
   descriptor-derived DAG + B0 reference executor
                           |
     B1 parallel  B2 cached  B3 cadence  P budgeted
                           |
       VALID / STALE / UNAVAILABLE / FAILED results
                           |
         replay + benchmark artifact outputs
                           v
  planned integration with original terminal's snapshot/API/UI
```

**No complete ASEP2-to-MetalArch production adapter or full frontend migration has yet been tested.** Reuse of the previous terminal remains a specified integration objective, not a completed Phase II deliverable.

### A. Native computational core and selected inherited workload

The first native C++20 prototype exposed eleven representative stages chosen from the earlier family taxonomy; the initial ten-repeat diagnostic below applies **only to that original eleven-stage cohort**. Its direct calculations include EMA(8/21) trend, Wilder RSI(14), RMS realized log-return volatility, average true range, timestamp-matched Pearson correlation for the peer, top-of-book volume imbalance and Goertzel spectral concentration. Four dependent heuristic stages—regime, risk, illustrative forecast and fusion—exercise downstream graph behavior. This *representative cohort* is deliberately smaller than the historical ASEP2 inventory; it is the measured Phase II workload, not a claim that all earlier analyses have been ported or that the illustrative forecast is empirically calibrated.

### B. Dependency, source and state contracts

A typed descriptor declares direct source streams, required upstream nodes, implementation revision, parameter identity and source/result age constraints. Registration rejects duplicate IDs, unknown/duplicate/self dependencies and cycles. The reference engine uses a deterministic topological order. Session-local stores have bounded event histories and versioned source-content identities; source reads are limited to the descriptor's declared inputs. Result cache identity combines engine/parameter/implementation identity, declared source revisions and upstream result identity, with independent age validation. Explicit result states are VALID, STALE, UNAVAILABLE and FAILED. Source provenance distinguishes OBSERVED, ESTIMATED and SIMULATED; in the fixture below all generated market observations are SIMULATED.

### C. Five execution policies

| Label | Implemented Phase II policy | Role in extension |
|---|---|---|
| B0 | Sequential, uncached, dependency-correct full recomputation | Reference against which candidates are compared |
| B1 | Dependency-wave parallel executor with persistent bounded workers and serial granularity fallback | Tests whether concurrent independent work is useful |
| B2 | Dependency-correct cache-aware recomputation subject to input/result-age limits | Tests repeated-work elimination without intentional output deferral |
| B3 | Correct fixed cadence with explicit last-known STALE display metadata | Evaluates the cadence concept already raised in the ASEP2 paper |
| P | Preliminary deterministic deadline-slack/critical-path ready queue under a declared nominal budget | Investigates resource-conditioned output selection |

The P cost estimates are **initial declared modeling inputs**, not yet a calibrated operating-system CPU or memory enforcement mechanism. The current prototype does not guarantee less computation or lower latency than B2.

### D. Replay and artifacts

The versioned MA1 trace records the source type, provider, symbol, timeframe, sequence, event time, ingestion time, numerical payload and provenance mode. Recorded events travel through the same source ingestion and policy execution paths as other session evaluations. The replay comparison deliberately excludes nondeterministic wall-clock profiling fields from normalized equality. Full market WebSocket capture, network fault recovery and multi-platform bitwise floating-point identity are **not yet established**. Raw performance artifacts and trace/source hashes are maintained separately from the paper's narrative.

## VI. Experimental Method and Initial Evidence

### A. Historical achievement versus new measurement

Earlier ASEP2 results demonstrate the reported *scope of an integrated terminal* (ingestion, analysis, serving, visual presentation, telemetry, optional acceleration). The current experiments instead measure the redesigned execution seam. They do **not** re-establish the prior paper's 71-engine claim, predictive value, live exchange semantics or desktop signing. Source claims remain version-bound.

### B. Initial Phase II verification

The completed local test suite reported **4,691 assertions across eight groups**, with three CTest targets passing under GCC Release, GCC ASan/UBSan Debug and Clang Release; a subsequent GitHub Actions matrix for the implementation branch succeeded on Ubuntu/GCC, Ubuntu/Clang and macOS/Clang. Tests cover DAG rejection, source contract enforcement, cache invalidation, result-status propagation, cadence deferral, policy comparison, session isolation and replay. These are implementation checks, not a proof of real-time production bounds. Independent 210-event synthetic P-policy CLI replays yielded an identical normalized output SHA-256 in the tested environment: `4c11150a5cd341b618d3fc397430bfc5ce15c978b8de54354f67a22927118821`.

### C. Initial five-policy diagnostic — synthetic only

The exploratory experiment uses a 300-bar, three-stream simulated workload, ten repetitions per policy, 65 warm-up bars, and 7,050 recorded event observations per policy. The results below are local ingestion-plus-computation times in a shared container, **not** controlled CPU quotas or observed live-feed end-to-end latency. All data are synthetic; measurements and the exact script are documented in `benchmarks/observations/`, `benchmarks/run_study.py` and `docs/BENCHMARK_NOTES.md`.

| Policy | p50 local time (µs) | p95 local time (µs) | Engine computations (10 runs) | Explicit deferrals |
|---|---:|---:|---:|---:|
| B0 full | 22.35 | 26.40 | 68,150 | 0 |
| B1 parallel | 24.15 | 33.39 | 68,150 | 0 |
| B2 cached | 17.36 | 26.79 | 28,200 | 0 |
| B3 cadence | 17.76 | 26.25 | 27,610 | 3,540 |
| P nominal budget = 8,000,000 | 21.98 | 27.78 | 28,200 | 4,700 |

B0, B1 and B2 agreed in normalized status and valid numerical values at matched cuts in this fixture; the recorded B3/P status differences are intentionally disclosed STALE outputs, not silent cache corruption. No output marked VALID violated its declared source-age limit in this specific test. On this low-cost workload, B1 was not faster than sequential B0. At the selected nominal budget, P deferred more outputs than B2 **without reducing computation count**. A short exploratory budget sweep offers feasibility data, but the present experiments do not demonstrate that P improves the constrained-resource trade-off. Pooled microsecond timing from this one environment is not a publication-grade systems evaluation.

Trace SHA-256: `d171a78b429ea06d6bd488e01bd62e8bb49bfc686ef4668d47dad05fd0978f1d`. The main raw CSV SHA-256 recorded by the prior study: `139838863e414738f507cf6e109d3d1a2d7c2e05f39804337601bd01636f3635` (raw CSV distributed in the separate phase-two source-and-results package; regenerate from the script).

### D. Audited extension of the original ASEP2 workload (Phase III)

To extend the previous semester's work beyond the original 11-stage native feasibility workload, we audited and adapted eight additional calculations from the historical ASEP2 modules: 12-bar rate of change, 14-bar Williams %R, 20-bar CCI, 20-bar Parkinson and Garman–Klass volatility, 20-return Amihud illiquidity, a 30-bar locally normalized OBV/volume-surge statistic, and a 40-return CUSUM statistic. These are **independently coded bounded-window C++20 mathematical features**, not a complete port of the prior engine registry, a rebranding of its 0–100 scores, or a claim of predictive improvement. The historical implementation's fixed annualization was omitted from the two range-based volatility features because instrument/timeframe semantics must first be independently verified. Zero denominators produce explicit UNAVAILABLE results where appropriate. The CUSUM port does **not** include Bayesian Online Change-Point Detection. Detailed definitions and lineage are in `docs/ENGINE_AUDIT.md` and the tests in `tests/test_legacy.cpp`.

The expanded cohort now comprises 19 executable stages. The original 11-stage cohort remains selectable and is exposed by the same five execution-policy implementations. A new CLI inventory enumerates each declared stage, dependencies and nominal cost; the figure-generation code derives the cohort size from the manifest instead of hard-coding an obsolete denominator. Local GCC Release, GCC ASan/UBSan and Clang Release each passed six CTest targets, including 4,723 existing-core assertions and 1,668 extension assertions across separate test binaries. These verify implementation contracts, not external empirical validity.

**Paired synthetic workload pilot.** Both cohorts replayed an identical 300-bar, three-stream trace (SHA-256 `d171a78b429ea06d6bd488e01bd62e8bb49bfc686ef4668d47dad05fd0978f1d`), with three repetitions per policy, 65 warm-up bars, and P nominal budget 8,000,000. Results are local ingestion-plus-compute timings on a shared host—not measured hard resource caps or observed-feed end-to-end latency. All output slots are counted over the three included runs.

| Cohort | Policy | Median local time (µs) | Computations | Valid slots | Explicit deferrals |
|---|---|---:|---:|---:|---:|
| 11-stage core | B0 | 17.93 | 20,445 | 20,445 | 0 |
| 11-stage core | B1 | 19.30 | 20,445 | 20,445 | 0 |
| 11-stage core | B2 | 13.73 | 8,460 | 20,445 | 0 |
| 11-stage core | B3 | 14.05 | 8,283 | 19,383 | 1,062 |
| 11-stage core | P | 17.41 | 8,460 | 19,035 | 1,410 |
| 19-stage expanded | B0 | 24.32 | 37,365 | 37,365 | 0 |
| 19-stage expanded | B1 | 26.69 | 37,365 | 37,365 | 0 |
| 19-stage expanded | B2 | 18.54 | 14,100 | 37,365 | 0 |
| 19-stage expanded | B3 | 18.98 | 13,923 | 36,303 | 1,062 |
| 19-stage expanded | P | 25.02 | 13,395 | 24,675 | 12,690 |

For each matched source cut in this fixture, B0/B1/B2 gave identical valid mathematical outputs/statuses and no unflagged freshness violation; P under the uncalibrated original budget lost a substantial number of available results as the cohort grew. An exploratory two-run expanded-cohort sensitivity test showed no extra P deferrals at 24-million nominal units, but **these units are descriptor-declared costs rather than enforced CPU nanoseconds**. Hence the results support feasibility and identify the need for measured-budget calibration; they do **not** establish that the preliminary P scheduler improves constrained-resource efficiency. Manifests, scripts and raw CSV are in the Phase III artifact; `docs/IMPLEMENTATION_PHASE3.md` records the full five-policy table and limitations. Neither the original 11-stage ten-run figures above nor the historical ASEP2 results have been silently reassigned to the expanded workload.

### D. Experiments required before stronger conclusions

The next evaluation must vary *measured or enforced* CPU limits, memory caps, instrument/event rates, engine cost distributions, worker counts, source delay and deadlines while holding the input trace/evaluation cuts constant across policies. The benchmark should report local p50/p95/p99, sustained rate, real CPU use, peak/steady memory, recomputation counts, source/output ages, missed freshness contracts, explicitly deferred slots, numerical disagreement with B0, scheduler overhead, bounded overload behavior and failure recovery. Add expensive original-family engines (for example spectral, volatility and cross-asset cases) with validated mathematics instead of increasing indicator count for appearance. Recorded observed-source traces require verified venue definitions, timestamps, acquisition rights and independently documented limitations. Performance conclusions and an updated abstract must follow the complete experiment—not precede it.

## VII. Discussion: What This Continuation Adds

The earlier ASEP2 accomplishment was integration of a broad, explainable research terminal on local infrastructure. MetalArch deepens its original efficiency requirement from an implementation consideration into an explicit systems research problem: what to execute, when a result can be safely reused, and how to represent useful-but-old evidence when the budget is exceeded. It supplies a testable native reference and reproducibility machinery to examine the issue rather than merely renaming the interface or announcing faster numerical kernels. The study also identifies when optimization **does not** pay off—particularly thread overhead on inexpensive stages and preliminary P-policy deferrals without corresponding work savings. Those negative findings are relevant constraints on the eventual design.

The current limits are substantial: the expanded 19-stage set is still smaller than ASEP2's historical analytical scope; the previous data/UI integration is not complete; cost estimates in P are nominal; CPU/memory limits have not been enforced in the reported study; event-recording fixtures do not recreate every live market-feed semantic; and no market-prediction or patentability claim is established. The extension's empirical strength depends on addressing these limits with fairly matched runs.

## VIII. Conclusion — provisional

Building on ASEP2 Metals Terminal's multi-engine financial research framework, MetalArch has established and tested an initial C++20 implementation for dependency-correct execution, selective recomputation, disclosed freshness, and deterministic replay, and extended the native workload from eleven to nineteen stages using audited adaptations of algorithms from the original ASEP2 project. The synthetic tests support internal contract checks and reference agreement for the non-deferred policies. They do not yet establish superiority of the preliminary freshness scheduler. The final paper will retain the previous semester's application motivation and credited engineering foundation, but its new quantitative claims will be confined to reproducible Phase II experiments.

## Manuscript and artifact provenance

- Original untouched paper: `paper/legacy/IEEE_RESEARCH_PAPER.md` and `paper/legacy/IEEE_RESEARCH_PAPER.tex` (June 2026; imported from user's handed-off local archive, **not** the connected ASEP2 GitHub `main` revision).
- Historical-source audit: `docs/LEGACY_AUDIT.md`.
- Previous standalone MetalArch methods draft: `paper/METALARCH_SYSTEMS_DRAFT_2026-10-02.md` (retained as an intermediate working version).
- New Phase II/III native source, fixture and experiments: implementation branch `impl/native-reference`, PR #1; relevant CMake, `src/`, `include/`, `tests/`, `benchmarks/` paths.
- Existing legacy bibliography is preserved verbatim in the archived manuscript. A unified IEEE-format bibliography requires source and relevance verification before submission. Do not simply copy citations to support new scheduling/novelty claims.
