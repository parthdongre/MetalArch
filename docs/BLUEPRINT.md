# MetalArch: Research and System Blueprint (PROPOSED v0)

Date: 2026-10-02. **This document describes intended work, not implemented MetalArch features or measured results.**

## Research objective and questions

Objective: investigate correctness-preserving, provenance-aware selective recomputation under freshness limits for heterogeneous analytics on one local machine.

- **RQ1 — Correctness:** Can DAG-derived execution and complete cache identity produce the same applicable outputs as uncached full recomputation on identical input snapshots, including parameter, peer, book, and upstream changes?
- **RQ2 — Freshness/compute trade-off:** Across the same workload and compute limits, how do full recompute, parallel recompute, caching, corrected fixed cadence, and freshness-aware scheduling compare on output age, deadline violations, CPU, latency and throughput?
- **RQ3 — Replay:** Can a recorded, immutable source-event trace reproduce the pipeline's normalized results and failure transitions when run with the same code, parameters, deterministic clock and controlled random seeds?
- **RQ4 — Validity/provenance:** Can explicit observed/estimated/simulated source metadata and validity propagation prevent unavailable or stale inputs from being presented as fresh, observed, valid analytical evidence?

These are research questions, not demonstrated benefits. A literature-gap/novelty claim requires a separate, sourced review.

## Execution path

```text
Historical/live adapters and recorded-input reader
             |
             v
Typed, versioned event envelope --> immutable trace + checksums
             |
             v
Validated source state / snapshots (one per execution session)
             |
             v
Descriptor registry --> DAG validation --> deterministic task plan
             |                                 |
             +---------------------------------+
                                               v
                Sequential reference / parallel / cache-aware executor
                                               |
                                               v
                                Provenance + validity-aware results
                                               |
                     +-------------------------+----------------------+
                     v                         v                      v
                Scheduler                 Benchmark sink        FastAPI / UI
                     |
       selected DAG closure (budget + freshness)
```

The executor, cache and result contracts must be identical in live and replay modes. The scheduler selects work but must not bypass dependency/validity rules.

## Source/event contracts

Define `SourceEvent` with: schema version, source/venue and instrument identity, event kind, event time, ingestion time, stable recording ordinal and provider sequence if available, payload, quality/mode metadata and integrity hash. Define duplicate handling and an explicit out-of-order/lateness policy; log dropped, coalesced, corrected and rejected events. Verify instrument names, semantics, time zone, licensing and source availability rather than assuming the legacy defaults are suitable.

The snapshot layer exposes **per-source immutable versions** and a consistent input-cut/watermark. Every accepted relevant input mutation changes the corresponding version. Derived window versions must also capture changes to historical observations used by an engine, not just the latest close or row count. Use incremental, content-verified versioning rather than rehashing every full history for each cycle. Keep source timestamps distinct from compute/arrival timestamps.

## Engine declaration and execution

A typed `EngineDescriptor` declares:
- Stable ID, callable/implementation digest and parameter schema/hash.
- Exact required and optional source streams, instruments, timeframes, window selectors, and required upstream engine IDs.
- Minimum data requirements, numerical tolerance, deterministic/random requirements.
- Freshness deadline / maximum accepted input age, result lifetime, and cost estimate.

At startup, reject duplicate names, missing required dependencies and cycles. Produce a deterministic topological ordering; only schedule ready nodes. Independent ready nodes may execute concurrently, but dependency edges form a barrier. Failure/unavailability propagates explicitly according to each node's declared policy; never instantiate a neutral numeric placeholder for a missing dependency.

Start with approximately 10-12 audited engines, not an unreviewed full migration: representative inexpensive indicators (trend, momentum, volatility), heavier statistics (realized volatility, GARCH or spectral), cross-asset analysis, a book-dependent method, regime/forecast/risk, and final fusion. First review dependencies and data semantics for each ported function. Synthetic macro approximations cannot be silently used as observed series. Retired experimental engines are outside the initial paper workload.

## Output contract

`AnalyticalResult` includes: engine/session ID, input-cut ID, upstream-result IDs, engine version, parameter hash, compute start/end, effective event time, computed-at time, source lineage and quality/mode flags, payload, score if applicable, diagnostics, and explicit status.

Status is one of **VALID**, **STALE**, **UNAVAILABLE**, **FAILED**. Numeric score is nullable. A legitimate neutral score of 50 is *not* an absence marker. Provenance mode for each input is **OBSERVED**, **ESTIMATED**, **SIMULATED**; a computed estimate based on an observed input must not itself be misrepresented as a directly observed measurement. Downstream fusion declares minimum evidence coverage and reports omitted/invalid inputs.

## Cache and isolation

Cache key: hash(engine ID + implementation digest + parameter digest + session/tenant namespace + primary/peer identity + selected timeframes/windows + exact versions of all declared source dependencies + exact versions/semantic IDs of required upstream results + deterministic execution settings). The absence of an optional input is part of key identity. Cache hits are permitted only for matching inputs and policy-compatible status. Keep a separate 'last known' display record to show a stale result without passing it off as a current computation. Every execution/replay session owns its source store, cache, clock, random state, scheduler state and counters; avoid mutable module globals.

## Reference and candidate policies

1. **B0:** correct sequential, uncached full recomputation on each evaluation point (reference).
2. **B1:** dependency-aware parallel, uncached recomputation.
3. **B2:** dependency-aware cache-aware execution, no freshness-based deferral.
4. **B3:** corrected fixed-cadence deferral with explicit STALE and source-version metadata.
5. **P:** proposed freshness-aware scheduler, which considers deadline, ready-node dependency closure, estimated execution cost, configurable per-cycle budget, and observed source updates.

Use an explicit deadline-slack priority (deadline minus evaluation time minus estimated remaining critical-path cost), with stable engine-ID tie breaking. Use cost estimates measured in a prior calibration window, never future samples. For exact replay of an adaptive live policy, record the cost-feedback/decision inputs and feed those through a virtual clock; otherwise freeze the calibrated cost table in the experiment manifest. Real wall-time performance measurements remain separate from logical replay time. Start with a clearly specified, deterministic tie-break policy and rolling cost estimates. Log selection, deferral, invalidation and deadline-miss reasons. Under overload use bounded queues, documented coalescing/backpressure and explicit invalid/stale outputs rather than silent drops or fabricated fresh results. When deferral intentionally changes a result from B0, record age and numerical/decision disagreement; do not call it exact equivalence.

## Replay and reproducibility

Record the **input stream**, not merely emitted engine scores. Replay passes input events through the same snapshot builder, DAG, cache and scheduler as live execution, using a virtual clock and deterministic tie rules. RNG seeds are explicit and per-engine/per-execution where relevant; reset state at run boundaries. Preserve input ordering/sequence and correction policy. Compare normalized outputs excluding intrinsically non-deterministic wall-time telemetry, using documented per-engine numerical tolerances.

Every experiment emits a manifest containing Git commit, dependency lock/hash, OS/Python/CPU/thread information, data-trace SHA256 and rights, source-schema version, engine set/parameters, policy, budget, worker count, seeds, warm-up/measurement windows and command line. Raw per-event CSV/Parquet/JSONL results, scripts and figures must be independently regenerable.

## Evaluation plan

Run all five policies on identical input traces and prescribed evaluation points. Separate observed/permissioned recorded traces from controlled synthetic fixtures and burst injections. Vary source rate, symbol count, engine cost/count, clients, workers, per-engine deadlines and compute budget. Include warm-up, steady state, repeated runs, uncertainty intervals, scheduler overhead and the same platform for within-platform policy comparisons.

Report event-to-publish p50/p95/p99 latency (with network delay separate), throughput, CPU time/utilization, peak/steady memory, queue/drop/coalescing counts, engine-level age and freshness violations, cache hit/miss/invalidation/wrong-reuse, valid-result numerical deviation and decision disagreement against B0 at matching input cuts, replay event/output agreement, and recovery from duplicate/out-of-order/delayed source events, disconnects and engine faults. Include parallel/cache/cadence/scheduler ablations. Never assert improvement until these runs are executed.

## Research interface

Keep FastAPI + WebSockets and selectively port the old frontend after the contract is stable. Display the dependency graph, result age/status and lineage, observed vs estimated vs simulated input modes, per-engine compute costs, scheduler decisions, replay comparison and manifest export. UI screenshots describe functionality, not system performance.

## Implementation sequence

| Phase | Work | Exit artifact |
|---|---|---|
| P0 | Evidence inventory, source licensing, legacy revision pin, manuscript claims ledger | Legacy audit |
| P1 | Typed source/result/engine contracts; strict DAG validator; 10-12 audited engines | Correct B0 oracle |
| P2 | Source snapshots/versions, isolated execution sessions, correct cache keys | B1+B2 with equivalence/invalidation tests |
| P3 | Immutable trace schema, writer/reader, virtual clock, seeded execution | Record/replay agreement |
| P4 | Benchmark harness/manifests; five policy interfaces; fixed cadence | Reproducible B0-B3 raw outputs |
| P5 | Proposed freshness-aware scheduling, cost estimator, overload handling | P policy, ablations, correctness and freshness data |
| P6 | Research-facing terminal and stress/failure work | Audited interface + failure data |
| P7 | Related-work verification and paper rewritten against real run artifacts | Source paper, bibliography, figures, validated manuscript |

See [LEGACY_AUDIT.md](LEGACY_AUDIT.md) for the revision boundary. No phase above is claimed complete solely because it appears in this plan.
