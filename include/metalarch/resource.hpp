#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <map>

namespace ma {
class Graph;
struct Descriptor;
// Advisory, process-wide observations. Current RSS and peak RSS are NOT an
// engine's private allocation footprint or an enforced memory limit.
struct ResourceSnapshot {
  uint64_t process_cpu_ns{0};
  uint64_t current_rss_bytes{0};
  uint64_t peak_rss_bytes{0};
  bool current_rss_supported{false};
  bool peak_rss_supported{false};
};
ResourceSnapshot sample_process_resources();
// CPU charged to the calling thread, including if it runs on a B1 worker.
// nullopt denotes an unsupported platform/clock; never fabricate zero CPU.
std::optional<uint64_t> sample_thread_cpu_ns();
// Stable non-cryptographic descriptor fingerprint for calibration compatibility;
// this does not replace the SHA-256 artifact manifest.
uint64_t descriptor_signature(const Descriptor& d);
// Validate every engine ID, revision, parameters, sample count and cost.
// An incomplete, extra, duplicate, stale or malformed calibration is rejected.
std::map<std::string,uint64_t> load_frozen_cost_table(const std::string& path,const Graph& graph);
}
