# MetalArch: Dependency-Correct Execution, Explicit Provenance, and Recorded-Input Replay for a Local Financial Analytics Pipeline

**Manuscript status (2 October 2026): systems-method draft, not publication-ready. The study of freshness-aware scheduling is proposed, not implemented or measured. No claim of patentability, novelty, predictive accuracy or statistically significant performance benefit is made.**

**Historical ASEP2 manuscript authors, original order (confirm contributions, inventorship if relevant, and affiliations before any submission):** Ramakrishna Bharsakde, Parth Dongre, Parth Birari, Atharv Patil, Aryan Patil. Original paper affiliation: Department of Engineering, Sciences and Humanities, Vishwakarma Institute of Technology, Pune, India. The revised manuscript must not silently treat the historic author list as a confirmed new-paper contribution statement.

## Abstract — provisional; update after full study

Multi-engine analytics applications often couple heterogeneous market inputs, numerical procedures, and derived outputs. Reusing intermediary results can reduce repeated computation, but a short input fingerprint or a manually ordered engine pipeline can silently invalidate dependent outputs. We describe the implementation of a C++20 reference prototype, MetalArch, that validates a declared engine graph, enforces source-read declarations, uses session-local and source-version-aware caching, and records explicit source provenance and analytical result status. A versioned synthetic input trace is replayed through the same analytical execution path as interactive evaluation. The initial implementation includes eleven representative computational and illustrative workload stages. Correctness-oriented unit and differential checks are provided, alongside a limited local sequential-versus-cache diagnostic. The proposed next investigation is whether a dependency-aware freshness scheduling policy provides a measurable computation/age trade-off against fair full-recompute, parallel, cache-aware and fixed-cadence baselines. The completed implementation does not yet establish this prospective research contribution.

**Keywords:** stream analytics; dependency graph; cache correctness; freshness; provenance; deterministic replay; reproducibility; precious metals.

## 1. Motivation and scope

The legacy ASEP2 Metals Terminal developed a broad collection of financial analyses and a web-based research interface. A separately supplied historical archive, rather than the connected GitHub main revision, contains a paper describing 71 registered engines and 38 passing tests. An independent handoff records a later local run of 92 passes with two warnings, and static inspection of that archive identifies 58 distinct imported registrations. These are **historical, revision-specific counts**, not MetalArch claims. The legacy orchestrator grouped engines into hand-assigned layers while dependency declarations were not the source of execution ordering; its cache used a small primary-frame fingerprint, and a global cadence could reuse results based on engine name without full context verification. Missing parents could be represented as default-scored results. These observations motivated a clean, limited system implementation rather than uncritical inheritance of the original engine collection.

MetalArch deliberately selects a small engineering workload to permit analytical correctness to be audited. No observed market feed, actual macroeconomic measurement, predictive performance or real-world exchange instrument semantics have yet been validated in this new implementation. Generated fixture events are labeled SIMULATED. This manuscript is about **software-system properties**, not a recommendation to trade gold or silver.

## 2. Research questions and hypotheses

**RQ1 (reference correctness).** Does descriptor-derived execution and cache invalidation preserve status and numerical output agreement with sequential uncached computation for equivalent source cuts? The test oracle must distinguish changes in peer/book inputs, declared parameters and required upstream results.

**RQ2 (freshness/compute trade-off, proposed).** At equal source cuts, defined freshness limits and controlled load, what computation cost, latency, output age and decision disagreement are produced by (B0) sequential full recompute, (B1) parallel full recompute, (B2) selective caching, (B3) fixed cadence with explicit stale values, and (P) a freshness-aware scheduler? There is **no presupposed winning policy**.

**RQ3 (recorded-input repeatability).** Under a pinned binary, virtual evaluation times and documented event ordering, can recording/replaying inputs reproduce the sequence of normalized output values, statuses and lineage identities?

**RQ4 (provenance).** Are observed, estimated and simulated upstream inputs distinguishable and can failed, unavailable and stale parents be prevented from masquerading as valid neutral evidence?

## 3. Formal model and contracts

Let the analytical graph be a directed acyclic graph \(G=(V,E)\), with each vertex \(i\) an analytical stage and an edge \((j,i)\) whenever stage \(i\) requires the output of stage \(j\). Each stage additionally declares a set of direct source streams \(S_i\), immutable implementation revision and parameter representation. A source stream is identified by provider, instrument, timeframe and kind (bar or book). At evaluation time \(t\), a stage may compute

\[
 y_i(t) = f_i( X_{S_i}(t),\ \{y_j(t):j\in\operatorname{pred}(i)\};\ \theta_i ).
\]

A valid computation requires the declared sources to have arrived by \(t\), all required parents to be valid, and an age contract to hold. Source event time and ingestion time are separately retained; knowing an event's event timestamp alone is insufficient to establish that it was causally observable. A result has one of four statuses: VALID, STALE, UNAVAILABLE, FAILED. A missing value is represented by absence, **not** a score of 50. Source lineage carries observed/estimated/simulated mode and per-stream version/content identity. Derived numerical values remain estimates even if their source readings are observed.

The current prototype forms a per-stage cache identity by combining engine ID, revision and parameter representation, declared age contract, source keys and their versions/rolling digests, and each declared parent's identity/status/mode. Versions are incremented on accepted source events and kept local to a Session. Its rolling digest is an FNV-style non-cryptographic identity aid; it is **not** a secure signature or an artifact integrity guarantee. Trace packages require SHA-256 from the release process. A retained cached result is also subject to the current freshness check before reuse.

## 4. Implementation

### 4.1 Event ingestion and retention

MetalArch has a standard-library-only C++20 computational core. Its MA1 text format encodes a version tag, kind, provider/instrument/timeframe identity, sequence, event nanoseconds, ingestion nanoseconds, five numeric payload fields, and provenance mode. Records with non-finite numbers, invalid OHLC or book constraints, conflicting duplicate latest sequence numbers, and decreasing sequence/event timestamps are rejected. Exact latest-sequence duplicates are idempotent. The current source store retains a configurable bounded history (default 4,096 events/stream), with minimum capacity 65 for the sample engine cohort. It does not yet implement late-event corrections, real market depth reconstruction or a durable append-only ingestion service.

### 4.2 Dependency execution and cache scope

Registration rejects duplicate stage IDs, unknown/self/duplicate required dependencies, duplicate declared direct inputs and cycles. Kahn's topological algorithm with stable lexical tie breaking produces the sequential reference order. A ReadView checks every attempted source read against the descriptor allowlist; engines receive only their declared parent results. Exceptions become FAILED results and cannot silently pass as valid. If a direct source becomes stale or a required parent is invalid, downstream computation does not proceed as if its numerical output were current. Each Session has an independent source store, cache and counters. External concurrent writes into an executing Session are not currently supported; callers must serialize ingestion and evaluation.

### 4.3 Representative workload and algorithms

This limited cohort was chosen to exercise distinct input costs and dependency patterns rather than maximize indicator count. Direct bar analyses are EMA(8/21) trend, Wilder RSI(14), 20-return root-mean-square log volatility and 14-period simple-average true range. Cross-asset analysis uses Pearson correlation on matched current **and preceding** bar timestamps; it returns unavailable when there are too few matched returns or a constant series. Book imbalance uses the latest top-of-book bid/ask quantities. Spectral concentration is estimated by applying the Goertzel recurrence to eight bins of 64 demeaned log returns; it is **not** represented as wavelet coherence.

The derived regime/risk/forecast/fusion stages apply declared deterministic heuristic functions. In particular, the component named `forecast` is an **illustrative weighted formula**, not a deployed or calibrated predictive model, and the fusion result is not a trading probability. Mathematical definitions and minimum-data constraints are itemized in `docs/ENGINE_AUDIT.md` alongside the source. The reference executor is presently sequential; the parallel baseline and proposed budget/freshness scheduler have not been implemented.

### 4.4 Replay semantics

The replay CLI parses MA1 lines, inserts accepted records through Store::ingest and executes the ordinary graph at each accepted event's ingestion timestamp. The normalized result text includes deterministic stage ID, status, source-input mode, optional numerical value, source timestamp, computation identity and reason; wall-clock benchmark telemetry is deliberately excluded from its equality comparison. Two runs on the same Linux build using a deterministic 210-event synthetic fixture produced identical output SHA-256 values (see below). This is a limited replay property, not an assertion of cross-platform floating-point bit identity or faithful live-feed reconstruction.

## 5. Initial verification and limited diagnostic results

All measurements in this section are **implementation smoke evidence**; they cannot resolve RQ2, predictive value or novelty.

### 5.1 Correctness tests

The release-mode GCC 14.2.0 and Clang 17 builds passed the native suite with **1,241 assertions across five test groups**. The same suite also completed under GCC AddressSanitizer and UndefinedBehaviorSanitizer in Debug mode. The checks cover topological ordering/cycle and undeclared dependency rejection, ingestion constraints and bounded retention, cache-versus-reference agreement, source and book-specific invalidation, session/reset isolation, an ingestion-time causality restriction, age expiry and status propagation, deliberate failure injection, and replay equivalence. This is not a proof of complete mathematical correctness or production fault tolerance.

For the 210-event wholly synthetic trace (`fixtures/synthetic_70.ma1`), two independent CLI replay outputs matched byte-for-byte on the tested build. SHA-256 of each normalized output was:

`e4e343ab4af23dc26b44adfdd77109d1d93afcd07fd6533bc6b42a5aaf4c10ae`.

### 5.2 Synthetic B0/B2 smoke timing

On a Linux x86_64 container reporting AMD EPYC 9V74 and GCC 14.2.0, ten consecutive runs of 300 timestamps with three simulated inputs per timestamp and two analytical evaluations per cut produced the following local timings. The measurement includes source generation/ingestion and computation, excludes network and I/O, and uses *no* controlled CPU pinning, randomized baseline order or confidence intervals.

| Policy | Median total elapsed time per 300-cut repeated-evaluation run | Min–max across 10 runs | Computations across all 10 runs | Cache hits |
|---|---:|---:|---:|---:|
| B0 sequential full | 8.934 ms | 8.617–9.606 ms | 64,400 | 0 |
| B2 session-local cache | 7.476 ms | 7.335–8.898 ms | 33,650 | 30,750 |

Both policies produced the same *rounded diagnostic numeric sink* (1372.329). Separate differential tests check normalized equality on shared input cuts. Since the benchmark intentionally repeats each snapshot, it grants B2 obvious reuse opportunities and cannot establish scheduling superiority or a meaningful speedup under live workload distributions. All sources in this benchmark were explicitly SIMULATED. `benchmarks/local_smoke_2026-10-02.txt` and `docs/BENCHMARK_NOTES.md` give the exact diagnostic outputs and caveats.

## 6. Remaining experimental study (not completed)

A publication-ready study should establish observed trace semantics and rights, artifact-level environment pinning, a correct independent B1 dependency-aware parallel policy, explicit B3 cadence/deferred outputs, and a precisely specified P freshness-aware selection algorithm. Every policy must use identical recorded inputs and aligned evaluation cuts. Workload dimensions should vary event arrival rate, symbol count, engine cost, worker count, freshness deadline, and resource budget. The measurements required are event-to-output latency distributions, per-engine source/result age, deadline violation rate, sustained throughput, CPU time, memory, queue lengths/drops, scheduling overhead and numerical/decision disagreement against B0. Experiments must include baseline ablations, failures, out-of-order/duplicate events, source disconnects and repeated runs with uncertainty summaries. A performance claim is defensible only if supported by the raw output and code revision.

## 7. Threats to validity and limits

This prototype currently processes deterministic **synthetic** streams, not independently verified venue data. It has eleven declared functions, not the original ASEP2 engine inventory. A fast digest is not collision-resistant proof. The default history is finite; EMA initialization uses the earliest retained value. The benchmark has only two completed policies (B0 and B2) and was performed in a shared container with uncontrolled resource contention. No external-source failure recovery, actual queue/backpressure implementation, real-time deadline scheduling, macOS/Apple Silicon verification, or validated predictive performance has been shown. Before journal/conference submission, the authors must verify paper citations and source rights, report failures and negative evidence honestly, and confirm manuscript contributions and affiliations.

## 8. Conclusion — provisional

A constrained C++20 reference implementation and validation fixtures are available for studying the relationship between dependency correctness, provenance, cache reuse and replay. The intended research on scheduling remains open. This paper's final claims, title and abstract must be revised after fair baseline comparisons and a systematic related-work evaluation.

## Artifact availability

Working draft implementation: `parthdongre/MetalArch`, branch `impl/native-reference`, draft PR #1 at the time of writing. Source files, CMake/CTest suite, CI definition, synthetic fixture generator, MA1 example trace, implementation audit and diagnostic notes are included. The original ASEP2 repository and historical manuscript are separate from this codebase.

## Related work to verify before final reference list

- Arasu et al., *Linear Road: A Stream Data Management Benchmark* (VLDB 2004), primary PDF: https://www.vldb.org/conf/2004/RS12P1.PDF
- Miao et al., *StreamBox: Modern Stream Processing on a Multicore Machine* (USENIX ATC 2017): https://www.usenix.org/conference/atc17/technical-sessions/presentation/miao
- *Benchmarking Distributed Stream Data Processing Systems* (arXiv:1802.08496), investigate methods and limits before citation: https://arxiv.org/abs/1802.08496
- ACM Artifact Review and Badging: https://www.acm.org/publications/policies/artifact-review-and-badging-current

A systematic comparison of incremental stream computation, provenance and freshness scheduling is still required. No claim that these works establish a unique MetalArch novelty gap is made.
