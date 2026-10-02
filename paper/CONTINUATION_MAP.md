# Research continuity: ASEP2 → MetalArch

This document clarifies that MetalArch is a **continuation and systems-engineering extension** of the earlier ASEP2 Metals Terminal, not an independent project that discards the prior semester's work. The previous paper remains archived verbatim alongside the integrated draft; material incorporated below is attributed as historical work.

| June 2026 ASEP2 source section | Earlier contribution, retained as foundation | Treatment in integrated continuation manuscript |
|---|---|---|
| Abstract and I. Introduction | Multi-engine local research terminal, transparent gold/silver analysis, undergraduate-resource deployment | Revised Abstract; I. Introduction: previous work and new constrained-execution question |
| II. Related Work | Accessible market analytics and classical indicator ecosystem | Retained as prior paper's cited context; independent systems literature review still required |
| III. Market Analysis Framework | Trend, momentum, volatility, participation, liquidity, cross-asset and scenarios; five interpretative questions | II.A: inherited application/workload semantics |
| IV. System Requirements | Local usability, bounded memory, expensive engine deferral, explainability and graceful fallback | III: motivation for quantified resource budgets and validity contracts |
| IV. Architecture; V. Data Layer | Python package, data adapters, bundles, FastAPI/WebSockets, JS UI, tick storage | II.B and V: earlier application architecture, new C++ execution seam, production bridge pending |
| VI. Engine Registry and Signal Model | Engine-family taxonomy and downstream fusion | II.A and V.A: same family taxonomy, current **11-stage representative native cohort**; historic and current engine counts not conflated |
| VII–VIII. Visualization and Monte Carlo | Earlier interface and simulation features | Phase I foundation; frontend not represented as ported into MetalArch |
| IX. Memory and Runtime Efficiency | Thread caps, slow cadence and bounded storage | III and V.C: new reference/cache/freshness policies, correct status disclosure |
| X. Desktop Packaging | Local deployment/installer approach | Historical deliverable; not claimed for current prototype |
| XI–XII. Testing and Results | Historic integration capabilities, manuscript's revision-specific tests | VI: distinct legacy claims, actual MetalArch correctness checks and limited synthetic timings |
| XIII–XV. Constraints, Enhancements, Conclusion | Previous terminal integration and limits | VII–VIII: explicit measured/unmeasured distinction, limits and remaining evaluation |
| Original References | Background for original financial-analytics paper | Original citation list preserved verbatim in archive; new systems bibliography to be independently verified |

## Claim-status rules

1. **Previous semester reported:** belongs to the preserved June manuscript, not automatically verified against the connected GitHub main revision.
2. **Re-audited legacy:** findings in `docs/LEGACY_AUDIT.md` apply to the inspected local archive/revision.
3. **Implemented Phase II:** only the components in native branch, tests and CI.
4. **Preliminary measurement:** only the labelled synthetic experiment with its trace/manifests; not observed-feed production latency.
5. **Planned:** frontend/data-adapter bridge, representative heavy-engine migration, CPU/memory quotas, calibrated scheduling, observed traces and publication-quality experiments.

**Original historical authors, unchanged in order:** Ramakrishna Bharsakde; Parth Dongre; Parth Birari; Atharv Patil; Aryan Patil. Coauthor confirmation and revised contribution statements are required before external submission.

Preserved originals (SHA-256):
- `paper/legacy/IEEE_RESEARCH_PAPER.md`: `5c653aa4bb91a8ef393bf5c0985fdb618de9d2fff197049437b9e8afb60f1ce5`.
- `paper/legacy/IEEE_RESEARCH_PAPER.tex`: `6c7a3d1598a4cb269af89cdb38d2db12d16fef8fc4064365f557c2eb13c971e8`.

## Further native workload extension, Phase III (2026-10-02)

The experimental C++ reference graph can now run either its original 11 stages or an expanded **19-stage** set with eight source-audited mathematical feature adaptations (`docs/ENGINE_AUDIT.md`). This remains a selected cohort, not the complete original ASEP2 collection. `paper/METALARCH_DRAFT.md` marks the prior 11-stage ten-run pilot separately from the new 11-vs-19 three-run synthetic pilot. The expanded prototype does not yet establish hard CPU-budget or memory-budget guarantees or validated production market-feed semantics. Reproduce the per-cohort stages via `metalarch_cli inventory core|expanded` and the paired diagnostic via `benchmarks/run_study.py --cohort`.
