# MetalArch

**Status: architecture proposal / research planning (2 October 2026). No new analytical implementation or MetalArch benchmarks have been completed.**

MetalArch is a proposed freshness-aware, provenance-tracked, reproducible local analytics system using precious-metal market research as its workload.

This repository is a **new implementation**, not a renamed copy of the legacy [ASEP2/Metals Terminal](https://github.com/parthdongre/ASEP2) repository. The original project remains untouched. Historical results and claims are not MetalArch measurements.

## Project objective

Study whether dependency-aware selective recomputation with explicit output-validity and freshness contracts changes computation cost, output age, and correctness versus well-defined baselines under identical recorded-input workloads.

The initial implementation must prioritize:
1. An uncached, dependency-correct reference executor.
2. Typed inputs, descriptors, source provenance, and explicit output validity.
3. Correct cache keys and isolated run/session state.
4. Recorded-input replay through the exact analytical execution path.
5. Fair, reproducible benchmark baselines.
6. A measured freshness-aware scheduling policy.

**Design specification:** [docs/BLUEPRINT.md](docs/BLUEPRINT.md)  
**Acceptance and benchmark protocol:** [docs/ACCEPTANCE.md](docs/ACCEPTANCE.md)  
**Legacy provenance and known issues:** [docs/LEGACY_AUDIT.md](docs/LEGACY_AUDIT.md)

## Research integrity

- Preserve the existing paper's listed authorship: Ramakrishna Bharsakde, Parth Dongre, Parth Birari, Atharv Patil, Aryan Patil; confirm affiliations/contributions before submission.
- Distinguish planned design, implemented capabilities, externally verified facts, and actual measured results.
- Do not assume any live exchange instrument or macro-feed semantics without independent validation.
- Synthetic workloads and simulated inputs must be separately labeled.
- No performance, predictive, novelty, or publication claims are made by this initial scaffold.

## Licensing and source migration

No blanket migration or relicensing of ASEP2 code or third-party assets has been performed. Any reused module must be audited, attributed, and covered by an appropriate license/data-use review.
