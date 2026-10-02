#pragma once
#include "metalarch/core.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace ma {
// MA2: canonical, line-oriented, append-only-before-finalization input record.
// SHA-256 is for integrity relative to an independently retained root, NOT authentication.
std::string sha256_hex(std::string_view input);
struct TraceMetadata {
  std::string dataset_id;       // user-supplied identity, not independently verified
  std::string origin;           // USER_CSV, MA1_MIGRATED, SYNTHETIC_FIXTURE
  std::string rights_ref;       // user-supplied rights/evidence pointer, never a license assertion
};
struct TraceSummary {
  uint64_t accepted{0}, duplicates{0};
  std::string root_sha256;
  uint64_t observed{0}, estimated{0}, simulated{0};
};
class TraceWriter {
public:
  TraceWriter(const std::filesystem::path& output, TraceMetadata metadata);
  ~TraceWriter();
  TraceWriter(const TraceWriter&)=delete;
  TraceWriter& operator=(const TraceWriter&)=delete;
  Ingest append(const Event& event);
  TraceSummary finish(); // finalize, then create destination without overwriting existing file
private:
  std::filesystem::path destination_,temp_;
  TraceMetadata metadata_;
  std::ofstream out_;
  Store validation_{65};
  int64_t global_ingest_ns_{0};
  std::string root_;
  TraceSummary summary_;
  bool finished_{false};
};
class TraceReader {
public:
  explicit TraceReader(const std::filesystem::path& file);
  bool next(Event& event); // false only after a fully verified END footer and trailing EOF
  const TraceMetadata& metadata() const {return metadata_;}
  const TraceSummary& summary() const;
private:
  std::ifstream in_;
  TraceMetadata metadata_;
  Store validation_{65};
  int64_t global_ingest_ns_{0};
  std::string root_;
  TraceSummary summary_;
  bool finished_{false};
};
// Strict one-source CSV with explicitly supplied provenance. No missing timestamps,
// guessed units, silent corrections, quoted fields or implicit observed attribution.
TraceSummary record_csv(const std::filesystem::path& csv,
  const std::filesystem::path& output, TraceMetadata metadata,
  const Key& key, Mode mode);
// Bundle TSV: source symbol timeframe kind mode csv_path rights_ref (tab-delimited).
// Files must each be ordered by ingest_ns; merge is O(streams) memory / O(log streams) per row.
// MA2 metadata binds SHA-256 of the complete companion source manifest.
TraceSummary record_csv_bundle(const std::filesystem::path& manifest,
  const std::filesystem::path& output, const std::string& dataset_id);
TraceSummary pack_ma1(const std::filesystem::path& ma1,
  const std::filesystem::path& output, TraceMetadata metadata);
}
