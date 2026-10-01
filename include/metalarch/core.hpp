#pragma once
#include <cstdint>
#include <algorithm>
#include <compare>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

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
struct Stream {
  std::vector<Event> events;
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
};
class Graph {
public:
  explicit Graph(std::vector<Descriptor> specs);
  const std::vector<Descriptor>& sorted() const { return sorted_; }
private:
  std::vector<Descriptor> sorted_;
};
class Session {
public:
  explicit Session(Graph graph): graph_(std::move(graph)) {}
  Store& store() { return store_; }
  const Store& store() const { return store_; }
  Results execute(int64_t now_ns, bool cache_enabled=true);
  void reset();
  uint64_t hits() const { return hits_; }
  uint64_t computations() const { return computations_; }
private:
  Graph graph_;
  Store store_;
  std::map<std::string,Result> memo_;
  uint64_t hits_{0}, computations_{0};
};
uint64_t fingerprint(const Descriptor& desc, const Store& store, const Results& parent);
std::string encode_event(const Event& e);
Event decode_event(const std::string& line);
std::string normalized_result(const Results& r);
std::string name(Status s);
std::string name(Mode s);
}