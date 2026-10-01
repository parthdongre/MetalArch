# Legacy evidence and migration boundary

**Status:** source audit note dated 2026-10-02; no MetalArch results are asserted here.

## Distinct source revisions

- Connected ASEP2 `main` inspected at commit `258862e83bfa4faa661618a619333c307039574e` (2026-06-17). This revision has a manually layered orchestrator, incomplete fingerprint, shared memo/cache cadence, and default-valued missing engine outputs.
- User-supplied `MetalArch_ChatGPT_Handoff.zip` is a different local snapshot: 129 files, SHA256 `d66f8dec313d5c14f2d19904e7a605bb3dc180b2f0801e7c8bca71c3645eb3dd`. It includes the old paper and tests absent from connected ASEP2 `main`.
- Manuscript: `docs/IEEE_RESEARCH_PAPER.md` in archive; SHA256 `5c653aa4bb91a8ef393bf5c0985fdb618de9d2fff197049437b9e8afb60f1ce5`.

## Audited legacy facts and caveats

The archived paper claims 71 registered engines and 38 passing tests. The handoff reports a later separate local run of 92 passes and 2 warnings, which has not been rerun as part of MetalArch. Static inspection of the supplied registry and registration decorators finds 58 distinct imported engine registrations. The counts describe different things/revisions and must never be conflated.

Snapshot examples of declared dependencies not respected by manually assigned execution layers: `forecast` layer 1 -> `regime` layer 1; `covar` layer 1 -> `risk` layer 2; `real_yield_model` layer 0 -> `macro_factor` layer 0; `ensemble_signal` layer 4 -> `signal` layer 4.

The old orchestrator's short fingerprint omits source/symbol/peer/book/full-window/parameter/upstream versions, and its slow cache reuses arbitrary engine-name matches. Global counters/memo interfere with isolation. Missing/failed outputs can resemble a valid neutral score of 50. The archived replay reconstructs empty frames and upstream context, not original market-input events. Two archived engine tests swallow failed assertions. Macro-like fallback series in beta/real-yield engines are simulated, not observed economic feeds. Old ML/feature targets and walk-forward return calculations need independent correction; old paper metrics are not accepted as new evidence.

No claim of measured latency/throughput, demonstrated scheduling benefit, predictive performance, novelty, desktop signing or live-feed rights is transferred to MetalArch.

## Credit and reuse

Historical paper authors in order: **Ramakrishna Bharsakde, Parth Dongre, Parth Birari, Atharv Patil, Aryan Patil**. Historical stated affiliation: DESH, Vishwakarma Institute of Technology, Pune; paper dated June 2026. Retain original-project attribution and confirm contribution/affiliation/consent prior to revised publication.

Keep ASEP2 unchanged. Audit license, third-party dependencies and data rights for every reused component. New MetalArch documentation commits are proposals, not an implementation of the described features.
