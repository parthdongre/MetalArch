# MetalArch M2 — Validated Multi-Source Input Recording and Tamper-Evident Replay

**Status:** Implemented and locally tested, 2 October 2026. This is an ASEP2 → MetalArch infrastructure extension, not proof of licensed observed-feed access, malicious-actor-proof storage, or improved scheduler throughput. The older MA1 fixture, 11/19-stage graphs, five execution policies, and original paper remain preserved.

## Motivation and completed scope

The ASEP2 terminal required heterogeneous inputs, while the Phase III/M1 MetalArch comparator intentionally accepted only SIMULATED fixture streams. M2 adds an independently validated, streaming source adapter and a new, bounded-memory recording protocol without altering mathematical engine results.

- `include/metalarch/trace.hpp` / `src/trace.cpp`: portable SHA-256 (validated with NIST standard empty/`abc`/one-million-`a` vectors); MA2 streaming `TraceWriter`/`TraceReader`; source provenance metadata; deterministic hash-chain and footer checks; strict no-overwrite finalization and cleanup of incomplete `.partial` output.
- `src/trace_main.cpp`: `metalarch_trace pack-ma1`, `record-csv`, `record-bundle`, and `verify` commands. All malformed/invalid/conflicting/out-of-order source events fail closed. Duplicate events are preserved in `D` audit rows rather than silently disappearing. The reader independently verifies `E` accepted and `D` idempotent-duplicate status against the original `Store::ingest` rules.
- `record-bundle`: a strict seven-column manifest declares separate keys (`source`, `symbol`, `timeframe`, `kind`), per-source provenance (`observed|estimated|simulated`), relative CSV paths and user-supplied rights references. At most 256 cursor streams are merged by ingestion time with a stable manifest-order tie break; only one current source row per cursor is retained in the merge queue. `Store` maintains at most 65 validation events per source. The MA2 header carries the companion manifest's SHA-256. Preserve the TSV alongside the recording and independently pin the MA2 root.
- `src/main.cpp`: recognizes MA2, fully verifies the input once **before** evaluating a graph, discovers the primary bar, peer bar and book keys from the recording without rebranding them as the simulated `fixture` provider, and replays accepted input events using the *same* Session/DAG/B0–B3/P logic as MA1. A second streaming read verifies footer integrity again. An optional external pinned root is supported. A missing peer/book remains explicitly UNAVAILABLE; no fabricated data or numerical placeholder is injected.

## Canonical MA2 wire protocol (v1)

One UTF-8/ASCII line per record with an LF terminator (no extra blank lines, comments, or CRLF):

```text
MA2|1|dataset_id|origin|rights_ref
E|ordinal|sha256_link|MA1|kind|source|symbol|timeframe|seq|event_ns|ingest_ns|a|b|c|d|e|mode
D|ordinal|sha256_link|MA1|kind|source|symbol|timeframe|seq|event_ns|ingest_ns|a|b|c|d|e|mode
END|accepted_count|verified_duplicate_count|final_sha256_root
```

`E` represents an accepted event and `D` an audited exact duplicate; both contribute to the ordinal and hash chain. `Mode`: `0=OBSERVED`, `1=ESTIMATED`, `2=SIMULATED`. `origin` is one of `USER_CSV`, `CSV_BUNDLE`, `MA1_MIGRATED`, and `SYNTHETIC_FIXTURE`. In a bundle, `rights_ref` in the MA2 header is exactly `sha256(source_manifest.tsv)`; the original per-stream rights references remain in the required companion file. `event_ns` and `ingest_ns` are *signed Unix nanoseconds*; they may never be silently synthesized from candle timestamps.

The initial chain value is `SHA256("MetalArch-MA2-v1\n" + canonical_header)`. Each accepted/duplicate record updates the link as `SHA256(previous_hex + "\n" + ordinal_decimal + "\n" + canonical_MA1_payload)`. The footer must match the reader's independently accumulated status counts and root. The `MA1` payload uses maximum round-trip precision; the decoder round-trip verifies canonical encoding. The sole sanctioned full-trace conversion is an explicit `pack-ma1`, preserving all original event modes and identities. A recording is installed using a same-directory temporary file and exclusive hard-link creation; an existing destination is never overwritten by this API.

**Threat model:** SHA-256 here detects accidental changes and partial/internal tampering of a pinned recording; a party able to rewrite the full file and its claimed root can still forge an entirely different chain. The root must therefore be retained **independently**, e.g. in a signed external submission/manifest. This is not a digital signature, immutable filesystem, validated market-data provider, or legal verification of feed rights. Another process can mutate the trace after finalization; the CLI verifies before replay and checks the second-pass footer, but concurrent file replacement and consumers that ignore nonzero exit codes remain outside the assurance boundary.

## Input contracts

Single-bar CSV header must be exactly:

```text
seq,event_ns,ingest_ns,open,high,low,close,volume
```

Single-book CSV header must be exactly:

```text
seq,event_ns,ingest_ns,bid,ask,bid_qty,ask_qty
```

Cells are unquoted strict ASCII finite decimals or integral nanoseconds, without local date strings, missing observations, embedded commas or implicit normalization. Bars use O/H/L/C/base volume; books use *snapshot* bid/ask and quantities only, **not** an exchange's full L2 delta protocol. The native store rejects inverted OHLC, negative quantities/volumes, nonfinite numbers, same-source out-of-order sequence/timestamps, conflicting duplicate sequence IDs, future events relative to ingress and unsupported source modes. An input's `OBSERVED` classification and its `rights_ref` are supplied by the CSV owner: MetalArch does not inspect licenses, call an exchange API or establish whether supplied timestamps reflect original packet arrival. Private/third-party datasets must not be distributed with the public repo unless authorized.

For a three-stream bundle the companion `source_manifest.tsv` columns are, in this exact order:

```text
source<TAB>symbol<TAB>timeframe<TAB>kind<TAB>mode<TAB>csv_path<TAB>rights_ref
```

Each CSV must already be sorted non-decreasing by its own `ingest_ns`. The bundle performs a stable k-way merge with `O(k)` current-row/queue memory and `O(log k)` selection per row, plus bounded per-source validation histories. Unexpected row errors abort output creation; the exclusive final install prevents accidental replacement of another recording.

## Reproduction with synthetic-only bundled fixture

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# Convert the original 210-event synthetic MA1 fixture to MA2.
./build/metalarch_trace pack-ma1 fixtures/synthetic_70.ma1 build/fixture-ma1.ma2 \
  legacy_demo synthetic.fixture

# Record three separately identified CSV streams into the same canonical MA2 protocol.
./build/metalarch_trace record-bundle \
  fixtures/m2_bundle/source_manifest.tsv build/fixture-bundle.ma2 synthetic_bundle

# Print and separately retain the final recorded SHA-256 root.
./build/metalarch_trace verify build/fixture-bundle.ma2

# Verify a pinned root AND the untouched companion source registry.
./build/metalarch_trace verify build/fixture-bundle.ma2 "$PINNED_ROOT" \
  fixtures/m2_bundle/source_manifest.tsv

# Replay into the native engine graph under all five existing execution policies.
./build/metalarch_cli replay build/fixture-bundle.ma2 b2 8000000 expanded - "$PINNED_ROOT"

# To ingest user-provided, authorized single-source CSV rather than fixtures:
./build/metalarch_trace record-csv input.csv build/user-input.ma2 dataset-01 \
  supplied.evidence.reference source-id XAU 1m bar observed
```

`record-*` refuses to replace its destination. Use a new path for each snapshot. The fixture and example generated by this project remain SIMULATED. No observed price data are included in M2's public source or example captures.

## Verification and limitations

- All **17 CTest targets** passed locally under GCC Release, Clang Release, and GCC ASan/UBSan Debug. B0/B1/B2/B3/P, old/expanded cohort smoke, prior M1 calibration, source validation, packing, bundle verification and recorded replay are included.
- Four test executables report 4,723 core + 1,668 legacy + 63 M1 + **935 new M2** assertions = **7,389**. Additional CTest integration checks are not double counted as assertions.
- A three-stream synthetic source manifest produced **210 accepted events, zero duplicates, 210 SIMULATED**, with provenance modes preserved, SHA-256 manifest/root independently checked. On this exact fixture all **10 combinations** (B0/B1/B2/B3/P × 11-/19-stage cohort) produced **byte-identical normalized analytical outputs** when compared to a direct MA1 replay on the same binary. This demonstrates migration correctness on a fixture, **not** real market validity or cross-platform bitwise identity.
- No measured CPU-budget/freshness improvement or production network-capture capability is attributed to this phase. For publication, the first authorized source trace must document venue/data rights, source timestamp semantics, sampling interval, instrument identity and recording time, followed by source checksum, pin and a fully matched per-policy benchmark.

## M3 handoff

Use these exact MA2 contracts to construct immutable per-cut source views, common computational primitives and incremental window updates. Before introducing faster rolling indicators, compare them to B0 mathematical oracles at matching verified MA2 cuts, including delayed/out-of-order and invalid input cases. Do not let the source adapter or replay logic bypass the freshness/provenance contract.
