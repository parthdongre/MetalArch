# Research manuscript outline (NOT READY FOR SUBMISSION)

Working title: MetalArch: Correctness, Provenance, and Freshness in a Reproducible Local Analytics Pipeline.

Original ASEP2 paper authors, retained pending confirmation of revised contributions: Ramakrishna Bharsakde; Parth Dongre; Parth Birari; Atharv Patil; Aryan Patil. Historical affiliation: DESH, Vishwakarma Institute of Technology, Pune.

## Abstract
To be written after the full experiments, related-work review and authorship approval. **No scheduling improvement, patent novelty, market forecasting accuracy or publication result is asserted.**

## Research questions
- RQ1: Under equal source cuts and age policy, can cached execution agree with the uncached, dependency-correct reference?
- RQ2: Can a freshness-aware scheduling policy alter computation cost and deadline-miss rates relative to fair full/cached/fixed-cadence policies?
- RQ3: Under documented stream schemas, corrections and seeds, can exact recorded-input replay reproduce valid output sequences?
- RQ4: Can explicit provenance and failure propagation avoid mislabeled observed/fresh downstream outputs?

## Implemented method (as of this snapshot)
C++20 deterministic graph; source-read allowlists; 11 analytical/illustrative stages; per-session versioned input cache with provenance; explicit result status; versioned MA1 trace reader; bounded input history and release/sanitizer tests. Separate output mode reflects *input provenance*, not direct observation of analytical results. For complete details and numerical definitions see ENGINE_AUDIT.md.

## Experimental method to complete before paper submission
B0 sequential full; B1 dependency-correct parallel full; B2 cached; B3 correct fixed cadence; P proposed freshness scheduler. Same recorded traces, pinned environment, predetermined evaluation points, varied pressure and deadline schedules, repeated randomized policy order, raw per-event exports, warm-up, error/status comparisons, and memory/CPU/latency/freshness data. Verify source instrument semantics and rights. Compare literature systematically before stating a research gap.

## Results status
Local B0/B2 smoke diagnostics use only synthetic source events and two evaluations per input cut; this is not an external market benchmark or complete RQ2 evaluation. Do not use unpublished percentages as a final abstract/result. Record negative or mixed outcomes. Future manuscript figures generated from raw artifacts only.