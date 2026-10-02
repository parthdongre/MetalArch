#pragma once
#include <cstdint>
#include <algorithm>
#include <compare>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "metalarch/resource.hpp"

namespace ma {
enum class Mode : uint8_t { Observed, Estimated, Simulated };
enum class Status : uint8_t { Valid, Stale, Unavailable, Failed };
enum class Ingest : uint8_t { Accepted, Duplicate, OutOfOrder, Invalid };
enum class Kind : uint8_t { Bar, Book };
struct Key {
  std::string source, symbol, timeframe;
  Kind kind{Kind::Bar};
  auto operator<=>(const Key&) const = default;
};
struct Event {
  Key key;
  uint64_t seq{0};
  int64_t event_ns{0}, ingest_ns{0};
  // Bar: open/high/low/close/volume; Book: bid/ask/bid_qty/ask_qty (first 4)
  double a{0}, b{0}, c{0}, d{0}, e{0};
  Mode mode{Mode::Observed};
};
// Bounded contiguous ring buffer: O(1) insertion after capacity, unlike
// std::vector::erase(begin()) which moves the full history each new event.
class EventWindow {
public:
  explicit EventWindow(size_t capacity=4096):capacity_(capacity){
    if(capacity==0)throw std::invalid_argument("event window capacity must be positive");
    data_.reserve(std::min(capacity,size_t{64})); // grow as needed: don't reserve 4,096 events per empty stream
  }
  size_t size() const noexcept {return data_.size();}
  size_t reserved_slots() const noexcept {return data_.capacity();}
  bool empty() const noexcept {return data_.empty();}
  const Event& operator[](size_t i) const {
    if(i>=data_.size())throw std::out_of_range("event window index");
    const size_t physical=head_+i;
    return data_[physical>=capacity_?physical-capacity_:physical];
  }
  const Event& front() const {return (*this)[0];}
  const Event& back() const {return (*this)[size()-1];}
  void push(const Event& e){
    if(data_.size()<capacity_){
      if(data_.size()==data_.capacity()){
        const size_t doubled=data_.capacity()>capacity_/2?capacity_:data_.capacity()*2;
        data_.reserve(std::min(capacity_,std::max(data_.capacity()+1,doubled)));
      }
      data_.push_back(e);
    }
    else {data_[head_]=e;head_=(head_+1)%capacity_;}
  }
private:
  std::vector<Event> data_;
  size_t capacity_,head_{0};
};
struct Stream {
  explicit Stream(size_t capacity=4096):events(capacity){}
  EventWindow events;
  uint64_t version{0};
  uint64_t digest{14695981039346656037ULL};
};
class Store {
public:
  explicit Store(size_t max_events_per_stream=4096): max_events_(max_events_per_stream) {
    if(max_events_per_stream<65) throw std::invalid_argument("history window must retain at least 65 events");
  }
  Ingest ingest(const Event& event);
  const Stream* get(const Key& key) const;
  uint64_t version(const Key& key) const;
  void reset() { streams_.clear(); }
  size_t streams() const { return streams_.size(); }
  // Excludes map/string/allocator overhead; this is only the reserved contiguous
  // storage of Event slots, useful for comparing lazy vs eager ring reservation.
  size_t reserved_event_payload_bytes_lower_bound() const {
    size_t count=0;
    for(const auto& [key,stream]:streams_){(void)key;count+=stream.events.reserved_slots();}
    return count*sizeof(Event);
  }
private:
  std::map<Key, Stream> streams_;
  size_t max_events_;
};
class ReadView {
public:
  ReadView(const Store& store,const std::vector<Key>& permitted): store_(store),permitted_(permitted) {}
  const Stream* get(const Key& key) const {
    if(std::find(permitted_.begin(),permitted_.end(),key)==permitted_.end())
      throw std::logic_error("undeclared engine source access");
    return store_.get(key);
  }
private:
  const Store& store_;
  const std::vector<Key>& permitted_;
};
struct Result {
  Status status{Status::Unavailable};
  Mode mode{Mode::Observed};
  std::optional<double> value;
  int64_t source_ns{0}, computed_ns{0};
  uint64_t identity{0};
  std::string reason;
  std::vector<std::string> lineage;
  std::vector<std::string> upstream_versions; // direct parent ID, result ID and status
  // When deferred, an old number is metadata only: never a VALID value.
  std::optional<double> last_known_value;
  uint64_t last_known_identity{0};
  int64_t last_known_source_ns{0}, last_known_computed_ns{0};
};
using Results = std::map<std::string,Result>;
struct Descriptor;
using Compute = std::function<Result(const ReadView&, const Results&)>;
struct Descriptor {
  std::string id, revision, parameters;
  std::vector<Key> sources;
  std::vector<std::string> dependencies;
  int64_t max_source_age_ns{0}; // 0 = no TTL; source age measured at evaluation time
  Compute compute;
  uint64_t estimated_cost_ns{1'000'000}; // deterministic calibration input, not an observed latency
  uint32_t cadence{1}; // 1 = always eligible; counted by evaluation cycles
  int64_t max_result_age_ns{0}; // 0 = no compute-age expiry
};
enum class Policy : uint8_t { SequentialFull, ParallelFull, Cache, FixedCadence, Freshness };
enum class CostModel : uint8_t { Declared, FrozenCalibration };
struct PolicyOptions {
  size_t workers{4};
  uint64_t compute_budget_ns{0}; // 0 = unlimited; estimated-cost accounting only
  uint64_t parallel_grain_ns{50'000}; // use serial for cheap waves; avoids thread overhead
  CostModel cost_model{CostModel::Declared};
  bool profile_resources{false}; // opt-in: profiler overhead is not silently charged to B0
};
struct EngineMeasurement {
  std::string id;
  Status status{Status::Unavailable};
  uint64_t wall_ns{0};
  std::optional<uint64_t> thread_cpu_ns; // nullopt where unsupported
};
struct CycleResources {
  uint64_t process_cpu_ns{0}; // sum of process CPU during this cycle, includes B1 workers
  uint64_t rss_before_bytes{0},rss_after_bytes{0},process_peak_rss_bytes{0};
  bool current_rss_supported{false},peak_rss_supported{false};
  size_t reserved_event_payload_bytes_lower_bound{0};
};
struct RunStats {
  uint64_t computations{0}, cache_hits{0}, deferred{0};
  uint64_t estimated_cost_ns{0}; // charged by selected cost model for actual compute calls
  uint64_t actual_compute_wall_sum_ns{0},actual_compute_thread_cpu_sum_ns{0};
  std::vector<EngineMeasurement> engine_measurements; // actual computed nodes only
  std::optional<CycleResources> resources; // process-wide, enabled only on demand
  size_t worker_queue_highwater{0}; // B1 pending task count, not an input-stream queue
  std::vector<std::string> execution_order; // deterministic publication order
};
class Graph {
public:
  explicit Graph(std::vector<Descriptor> specs);
  const std::vector<Descriptor>& sorted() const { return sorted_; }
private:
  std::vector<Descriptor> sorted_;
};
class WorkerPool;
class Session {
public:
  explicit Session(Graph graph);
  ~Session();
  Session(const Session&)=delete;
  Session& operator=(const Session&)=delete;
  Store& store() { return store_; }
  const Store& store() const { return store_; }
  Results execute(int64_t now_ns, bool cache_enabled=true); // compatibility B0/B2 API
  Results execute(int64_t now_ns, Policy policy, PolicyOptions options={});
  const RunStats& last_run() const { return last_run_; }
  // Complete, precomputed cost table: no online timing can influence a frozen
  // replay's scheduling choices. Validation rejects unknown/missing/zero IDs.
  void set_frozen_costs(const std::map<std::string,uint64_t>& costs);
  const std::map<std::string,uint64_t>& frozen_costs() const {return frozen_cost_ns_;}
  void reset();
  uint64_t hits() const { return hits_; }
  uint64_t computations() const { return computations_; }
private:
  Graph graph_;
  Store store_;
  std::map<std::string,Result> memo_;
  uint64_t hits_{0}, computations_{0}, cycles_{0};
  int64_t last_eval_ns_{0};
  RunStats last_run_;
  std::map<std::string,uint64_t> measured_cost_ns_; // advisory only, never changes semantic output
  std::map<std::string,uint64_t> frozen_cost_ns_; // user-selected, immutable during execution
  std::unique_ptr<WorkerPool> pool_;
  size_t pool_workers_{0};
};
uint64_t fingerprint(const Descriptor& desc, const Store& store, const Results& parent);
std::string encode_event(const Event& e);
Event decode_event(const std::string& line);
std::string normalized_result(const Results& r);
std::string name(Status s);
std::string name(Mode s);
}
