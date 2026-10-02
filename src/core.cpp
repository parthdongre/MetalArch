#include "metalarch/core.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <numeric>
#include <memory>
#include <tuple>
#include <limits>
#include <locale>
#include <queue>
#include <set>
#include <sstream>
#include <utility>
namespace ma {
namespace {
constexpr uint64_t OFFSET=14695981039346656037ULL, PRIME=1099511628211ULL;
void mix(uint64_t& h, uint64_t v) { for (int n=0;n<8;++n) {h ^= (v & 255U); h *= PRIME; v >>= 8U;} }
void mix(uint64_t& h, const std::string& s) { mix(h, s.size()); for(char c:s){h^=static_cast<unsigned char>(c);h*=PRIME;} }
std::vector<std::string> parts(const std::string& s) {
  std::vector<std::string> p; size_t start=0;
  for(size_t pos=0;pos<=s.size();++pos) if(pos==s.size()||s[pos]=='|') {p.push_back(s.substr(start,pos-start));start=pos+1;}
  return p;
}
int64_t parse_i(const std::string& s) {size_t end=0;auto v=std::stoll(s,&end);if(end!=s.size())throw std::invalid_argument("invalid integer");return v;}
uint64_t parse_u(const std::string& s) {size_t end=0;auto v=std::stoull(s,&end);if(end!=s.size()||s[0]=='-')throw std::invalid_argument("invalid unsigned");return v;}
double parse_d(const std::string& s) {size_t end=0;auto v=std::stod(s,&end);if(end!=s.size()||!std::isfinite(v))throw std::invalid_argument("invalid float");return v;}
}
// Session-owned bounded worker pool; unlike per-event std::async, workers
// persist across execution cycles. No worker can mutate Session/Store/memo.
class WorkerPool {
public:
  explicit WorkerPool(size_t workers){
    for(size_t i=0;i<workers;++i)threads_.emplace_back([this](){
      for(;;){
        std::function<void()> task;
        {
          std::unique_lock lock(mutex_);
          cv_.wait(lock,[&]{return stopping_||!tasks_.empty();});
          if(stopping_&&tasks_.empty())return;
          task=std::move(tasks_.front());tasks_.pop_front();
        }
        task();
      }
    });
  }
  ~WorkerPool(){
    {std::lock_guard lock(mutex_);stopping_=true;}
    cv_.notify_all();
    for(auto& thread:threads_)thread.join();
  }
  WorkerPool(const WorkerPool&)=delete;
  WorkerPool& operator=(const WorkerPool&)=delete;
  template <class F> auto submit(F&& work)->std::future<decltype(work())>{
    using Return=decltype(work());
    auto job=std::make_shared<std::packaged_task<Return()>>(std::forward<F>(work));
    auto answer=job->get_future();
    {
      std::lock_guard lock(mutex_);
      if(stopping_)throw std::logic_error("worker pool is closing");
      tasks_.emplace_back([job](){(*job)();});
      queue_highwater_=std::max(queue_highwater_,tasks_.size());
    }
    cv_.notify_one();
    return answer;
  }
  void reset_highwater(){std::lock_guard lock(mutex_);queue_highwater_=tasks_.size();}
  size_t highwater(){std::lock_guard lock(mutex_);return queue_highwater_;}
private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::function<void()>> tasks_;
  std::vector<std::thread> threads_;
  bool stopping_{false};
  size_t queue_highwater_{0};
};
Session::Session(Graph graph):graph_(std::move(graph)){}
Session::~Session()=default;
void Session::set_frozen_costs(const std::map<std::string,uint64_t>& costs){
  if(costs.size()!=graph_.sorted().size())
    throw std::invalid_argument("calibration must cover every registered engine");
  for(const auto& d:graph_.sorted()){
    const auto it=costs.find(d.id);
    if(it==costs.end() || it->second==0)
      throw std::invalid_argument("missing or zero calibration cost: "+d.id);
  }
  frozen_cost_ns_=costs;
}
Ingest Store::ingest(const Event& e) {
  if(static_cast<uint8_t>(e.key.kind)>static_cast<uint8_t>(Kind::Book) ||
     static_cast<uint8_t>(e.mode)>static_cast<uint8_t>(Mode::Simulated) ||
     e.key.source.empty() || e.key.symbol.empty() || e.key.timeframe.empty() ||
     e.key.source.find_first_of("|\r\n")!=std::string::npos ||
     e.key.symbol.find_first_of("|\r\n")!=std::string::npos ||
     e.key.timeframe.find_first_of("|\r\n")!=std::string::npos ||
     e.seq==0 || e.event_ns<=0 || e.ingest_ns<=0 || e.event_ns>e.ingest_ns ||
     !std::isfinite(e.a)||!std::isfinite(e.b)||!std::isfinite(e.c)||!std::isfinite(e.d)||!std::isfinite(e.e) ||
     (e.key.kind==Kind::Bar && (e.a<=0||e.b<e.a||e.b<e.d||e.c>e.a||e.c>e.d||e.c<=0||e.d<=0||e.e<0)) ||
     (e.key.kind==Kind::Book && (e.a<=0||e.b<e.a||e.c<0||e.d<0))) return Ingest::Invalid;
  auto& s=streams_.try_emplace(e.key,max_events_).first->second;
  if(!s.events.empty()){
    const Event& prev=s.events.back();
    if(e.seq==prev.seq){
      // Exact duplicate is idempotent; divergent same-sequence event is rejected.
      return encode_event(e)==encode_event(prev)?Ingest::Duplicate:Ingest::Invalid;
    }
    if(e.seq<prev.seq || e.event_ns<prev.event_ns || e.ingest_ns<prev.ingest_ns) return Ingest::OutOfOrder;
  }
  s.events.push(e);
  ++s.version;
  // Rolling source-content digest: version alone is not enough to identify traces.
  const auto encoded=encode_event(e);
  for(char ch:encoded){s.digest^=static_cast<unsigned char>(ch);s.digest*=PRIME;}
  return Ingest::Accepted;
}
const Stream* Store::get(const Key& k) const {auto it=streams_.find(k);return it==streams_.end()?nullptr:&it->second;}
uint64_t Store::version(const Key& k) const {auto s=get(k);return s?s->version:0;}
Graph::Graph(std::vector<Descriptor> specs) {
  std::map<std::string,Descriptor> by_id;
  for(auto& d:specs){
    const std::string id=d.id;
    if(id.empty()||d.revision.empty()||!d.compute||d.max_source_age_ns<0||d.max_result_age_ns<0||d.cadence==0||d.estimated_cost_ns==0||!by_id.emplace(id,std::move(d)).second)
      throw std::invalid_argument("invalid/duplicate engine descriptor");
  }
  std::map<std::string,std::vector<std::string>> adj;
  std::map<std::string,size_t> degree;
  for(const auto& [name,d]:by_id){
    degree[name]=0;
    std::set<std::string> seen;
    std::set<Key> seen_sources;
    for(const auto& key:d.sources)if(!seen_sources.insert(key).second)throw std::invalid_argument("duplicate declared source for "+name);
    for(const auto& req:d.dependencies){
      if(!by_id.contains(req)||req==name||!seen.insert(req).second) throw std::invalid_argument("unknown/self/duplicate required dependency: "+name+" -> "+req);
      ++degree[name]; adj[req].push_back(name);
    }
  }
  std::priority_queue<std::string,std::vector<std::string>,std::greater<>> ready;
  for(const auto& [name,deg]:degree) if(deg==0) ready.push(name);
  while(!ready.empty()) {
    auto name=ready.top();ready.pop();
    sorted_.push_back(by_id.at(name));
    for(const auto& child:adj[name]) if(--degree[child]==0)ready.push(child);
  }
  if(sorted_.size()!=by_id.size()) throw std::invalid_argument("cycle in engine dependency graph");
}
uint64_t fingerprint(const Descriptor& d,const Store& s,const Results& parents){
  uint64_t h=OFFSET;mix(h,d.id);mix(h,d.revision);mix(h,d.parameters);mix(h,static_cast<uint64_t>(d.max_source_age_ns));mix(h,static_cast<uint64_t>(d.max_result_age_ns));
  for(const auto& k:d.sources){mix(h,k.source);mix(h,k.symbol);mix(h,k.timeframe);mix(h,static_cast<uint64_t>(k.kind));mix(h,s.version(k));
    if(const auto* stream=s.get(k))mix(h,stream->digest);}
  for(const auto& id:d.dependencies){
    const auto it=parents.find(id);if(it==parents.end())throw std::logic_error("parent was not executed");
    mix(h,id);mix(h,it->second.identity);mix(h,static_cast<uint64_t>(it->second.status));
    mix(h,static_cast<uint64_t>(it->second.mode));
  }
  return h;
}
// The reference and every candidate policy share one evaluation path.  Work is
// published in deterministic order; B1 only overlaps independent ready nodes.
Results Session::execute(int64_t now_ns,bool cache_enabled) {
  return execute(now_ns,cache_enabled?Policy::Cache:Policy::SequentialFull);
}
Results Session::execute(int64_t now_ns,Policy policy,PolicyOptions options) {
  if(now_ns<=0 || (last_eval_ns_!=0 && now_ns<last_eval_ns_))
    throw std::invalid_argument("invalid / backwards evaluation timestamp; reset before replay");
  if(options.workers==0 || options.workers>256)throw std::invalid_argument("workers must be in 1..256");
  if(policy==Policy::Freshness && options.cost_model==CostModel::FrozenCalibration &&
     frozen_cost_ns_.size()!=graph_.sorted().size())
    throw std::invalid_argument("frozen P policy requires a complete calibration table");
  if(options.cost_model!=CostModel::Declared && options.cost_model!=CostModel::FrozenCalibration)
    throw std::invalid_argument("unknown cost model");
  const ResourceSnapshot profile_start=options.profile_resources?sample_process_resources():ResourceSnapshot{};
  last_eval_ns_=now_ns;
  ++cycles_;
  last_run_={};
  const auto& ordered=graph_.sorted();
  Results outputs;
  const auto cost_of=[&](const Descriptor& d)->uint64_t{
    if(policy==Policy::Freshness && options.cost_model==CostModel::FrozenCalibration)
      return frozen_cost_ns_.at(d.id);
    return d.estimated_cost_ns;
  };
  const auto finalize=[&](){
    if(!options.profile_resources)return;
    const auto end=sample_process_resources();
    CycleResources cycle;
    cycle.process_cpu_ns=end.process_cpu_ns>=profile_start.process_cpu_ns?
      end.process_cpu_ns-profile_start.process_cpu_ns:0;
    cycle.rss_before_bytes=profile_start.current_rss_bytes;
    cycle.rss_after_bytes=end.current_rss_bytes;
    cycle.process_peak_rss_bytes=end.peak_rss_bytes;
    cycle.current_rss_supported=profile_start.current_rss_supported && end.current_rss_supported;
    cycle.peak_rss_supported=end.peak_rss_supported;
    cycle.reserved_event_payload_bytes_lower_bound=store_.reserved_event_payload_bytes_lower_bound();
    last_run_.resources=cycle;
  };
  const bool cache_policy=(policy==Policy::Cache||policy==Policy::FixedCadence||policy==Policy::Freshness);
  struct Outcome {
    Result result; bool cache_hit{false}; bool computed{false};
    uint64_t measured_compute_ns{0};std::optional<uint64_t> measured_thread_cpu_ns;
  };

  // No mutation of Store, memo, outputs or counters occurs in this evaluator.
  // That is the essential B1 parallelism invariant.
  const auto evaluate=[&](const Descriptor& d,const Results& upstream,bool allowed,bool reuse)->Outcome {
    Outcome out;
    auto& r=out.result;
    bool blocked=false;
    std::string reason;
    int64_t source_ns=std::numeric_limits<int64_t>::max();
    Mode mode=Mode::Observed;
    std::vector<std::string> lineage;
    std::vector<std::string> upstream_versions;
    for(const auto& key:d.sources){
      const auto* stream=store_.get(key);
      if(!stream||stream->events.empty()){
        if(!blocked)reason="source unavailable";
        blocked=true;
        lineage.push_back(key.source+":"+key.symbol+":"+key.timeframe+":UNAVAILABLE");
        continue;
      }
      const auto& ev=stream->events.back();
      lineage.push_back(key.source+":"+key.symbol+":"+key.timeframe+":"+
                        std::to_string(stream->version)+":"+std::to_string(stream->digest));
      mode=std::max(mode,ev.mode);
      if(ev.event_ns>now_ns || ev.ingest_ns>now_ns){
        if(!blocked)reason="source not available at evaluation time";
        blocked=true;continue;
      }
      source_ns=std::min(source_ns,ev.event_ns);
    }
    for(const auto& parent:d.dependencies){
      const auto& p=upstream.at(parent);
      if(p.source_ns>0)source_ns=std::min(source_ns,p.source_ns);
      mode=std::max(mode,p.mode);
      lineage.insert(lineage.end(),p.lineage.begin(),p.lineage.end());
      upstream_versions.push_back(parent+":"+std::to_string(p.identity)+":"+name(p.status));
      if(p.status!=Status::Valid){
        if(!blocked)reason="upstream "+parent+" is "+name(p.status);
        blocked=true;
      }
    }
    std::sort(lineage.begin(),lineage.end());
    lineage.erase(std::unique(lineage.begin(),lineage.end()),lineage.end());
    const auto signature=fingerprint(d,store_,upstream);
    r.identity=signature;
    r.mode=mode;
    r.lineage=lineage;
    r.upstream_versions=upstream_versions;
    r.source_ns=(source_ns==std::numeric_limits<int64_t>::max()?0:source_ns);
    const auto it=memo_.find(d.id);
    // Present an old result only in last_known_value metadata, never as a
    // VALID value for consumption by downstream engines.
    auto attach_last_known=[&](){
      if(it!=memo_.end() && it->second.status==Status::Valid){
        r.last_known_value=it->second.value;
        r.last_known_identity=it->second.identity;
        r.last_known_source_ns=it->second.source_ns;
        r.last_known_computed_ns=it->second.computed_ns;
      }
    };
    if(blocked){r.status=Status::Unavailable;r.reason=reason;return out;}
    if(d.max_source_age_ns>0 && r.source_ns>0 && now_ns-r.source_ns>d.max_source_age_ns){
      r.status=Status::Stale;r.reason="source age exceeds declared limit";attach_last_known();return out;
    }
    if(reuse&&it!=memo_.end()&&it->second.status==Status::Valid&&it->second.identity==signature &&
       it->second.computed_ns<=now_ns &&
       (d.max_result_age_ns==0 || now_ns-it->second.computed_ns<=d.max_result_age_ns)){
      r=it->second;
      out.cache_hit=true;
      return out;
    }
    if(!allowed){
      r.status=(it==memo_.end()?Status::Unavailable:Status::Stale);
      r.reason="compute deferred by execution policy";
      attach_last_known();
      return out;
    }
    out.computed=true;
    std::optional<uint64_t> thread_start;
    if(options.profile_resources)thread_start=sample_thread_cpu_ns();
    const auto compute_start=std::chrono::steady_clock::now();
    try{
      ReadView view(store_,d.sources);
      Results parents;
      for(const auto& parent:d.dependencies)parents.emplace(parent,upstream.at(parent));
      r=d.compute(view,parents);
    }catch(const std::exception& ex){r=Result{};r.status=Status::Failed;r.reason=ex.what();}
    catch(...){r=Result{};r.status=Status::Failed;r.reason="unknown engine exception";}
    out.measured_compute_ns=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now()-compute_start).count());
    if(thread_start.has_value()){
      const uint64_t first_cpu=*thread_start;
      const auto thread_end=sample_thread_cpu_ns();
      if(thread_end.has_value() && *thread_end>=first_cpu)
        out.measured_thread_cpu_ns=*thread_end-first_cpu;
    }
    if(r.status==Status::Valid && (!r.value || !std::isfinite(*r.value))){
      r.status=Status::Failed;r.value.reset();r.reason="valid result has no finite value";
    }
    r.identity=signature;
    r.mode=mode;
    r.lineage=std::move(lineage); // evaluated source provenance, not engine-supplied overrides
    r.upstream_versions=std::move(upstream_versions);
    r.source_ns=(source_ns==std::numeric_limits<int64_t>::max()?0:source_ns);
    r.computed_ns=now_ns; // controlled logical clock; wall time measured separately
    if(r.status!=Status::Valid)r.value.reset();
    r.last_known_value.reset();r.last_known_identity=0;
    r.last_known_source_ns=r.last_known_computed_ns=0;
    return out;
  };
  auto publish=[&](const Descriptor& d,Outcome&& out){
    if(out.cache_hit){++hits_;++last_run_.cache_hits;}
    if(out.computed){
      ++computations_;++last_run_.computations;
      const uint64_t sample=std::max<uint64_t>(1,out.measured_compute_ns);
      auto& estimate=measured_cost_ns_[d.id];
      estimate=(estimate==0?sample:(estimate/8)*7+(sample/8));
      const uint64_t charge=cost_of(d);
      if(UINT64_MAX-last_run_.estimated_cost_ns<charge)
        throw std::overflow_error("estimated cost overflow");
      last_run_.estimated_cost_ns+=charge;
      if(UINT64_MAX-last_run_.actual_compute_wall_sum_ns<out.measured_compute_ns)
        throw std::overflow_error("wall-time sample overflow");
      last_run_.actual_compute_wall_sum_ns+=out.measured_compute_ns;
      if(out.measured_thread_cpu_ns){
        if(UINT64_MAX-last_run_.actual_compute_thread_cpu_sum_ns<*out.measured_thread_cpu_ns)
          throw std::overflow_error("thread CPU sample overflow");
        last_run_.actual_compute_thread_cpu_sum_ns+=*out.measured_thread_cpu_ns;
      }
      if(options.profile_resources)last_run_.engine_measurements.push_back(
        {d.id,out.result.status,out.measured_compute_ns,out.measured_thread_cpu_ns});
      if(cache_policy && out.result.status==Status::Valid)memo_[d.id]=out.result;
    }
    if(out.result.reason=="compute deferred by execution policy")++last_run_.deferred;
    last_run_.execution_order.push_back(d.id);
    outputs.emplace(d.id,std::move(out.result));
  };
  if(policy==Policy::ParallelFull){
    if(pool_)pool_->reset_highwater();
    // Waves derived from actual declared dependencies, never manually assigned.
    std::map<std::string,size_t> level;
    std::vector<std::vector<size_t>> waves;
    for(size_t i=0;i<ordered.size();++i){
      size_t l=0;
      for(const auto& parent:ordered[i].dependencies)l=std::max(l,level.at(parent)+1);
      level.emplace(ordered[i].id,l);
      if(waves.size()<=l)waves.resize(l+1);
      waves[l].push_back(i);
    }
    for(const auto& wave:waves){
      for(size_t offset=0;offset<wave.size();offset+=options.workers){
        const size_t count=std::min(options.workers,wave.size()-offset);
        uint64_t expected_ns=0;
        for(size_t j=0;j<count;++j){
          const auto& d=ordered[wave[offset+j]];
          const auto measured=measured_cost_ns_.find(d.id);
          const uint64_t estimate=(measured==measured_cost_ns_.end()?d.estimated_cost_ns:measured->second);
          expected_ns=(UINT64_MAX-expected_ns<estimate?UINT64_MAX:expected_ns+estimate);
        }
        if(count<2 || options.workers==1 || expected_ns<options.parallel_grain_ns){
          for(size_t j=0;j<count;++j){
            const auto& d=ordered[wave[offset+j]];
            publish(d,evaluate(d,outputs,true,false));
          }
          continue;
        }
        if(!pool_ || pool_workers_!=options.workers){
          pool_=std::make_unique<WorkerPool>(options.workers);
          pool_workers_=options.workers;
        }
        std::vector<std::future<Outcome>> pending;
        pending.reserve(count);
        for(size_t j=0;j<count;++j){
          const Descriptor* d=&ordered[wave[offset+j]];
          pending.push_back(pool_->submit([&,d](){return evaluate(*d,outputs,true,false);}));
        }
        // No publication until all concurrently running peers have completed.
        std::vector<Outcome> ready;
        ready.reserve(count);
        for(auto& f:pending)ready.push_back(f.get());
        for(size_t j=0;j<count;++j)publish(ordered[wave[offset+j]],std::move(ready[j]));
      }
    }
    if(pool_)last_run_.worker_queue_highwater=pool_->highwater();
    finalize();return outputs;
  }
  if(policy==Policy::Freshness){
    // A bounded-budget ready queue: derive a node's deadline from its own
    // sources AND upstream deadlines. Ancestors inherit urgent descendant
    // deadlines. Critical-path cost breaks urgency ties deterministically.
    const size_t n=ordered.size();
    std::map<std::string,size_t> index;
    for(size_t i=0;i<n;++i)index.emplace(ordered[i].id,i);
    std::vector<std::vector<size_t>> children(n);
    std::vector<size_t> degree(n,0);
    const int64_t INF=std::numeric_limits<int64_t>::max();
    std::vector<int64_t> due(n,INF), earliest(n,INF);
    std::vector<uint64_t> critical(n,0);
    for(size_t i=0;i<n;++i){
      const auto& d=ordered[i];
      int64_t first=INF;
      for(const auto& k:d.sources){
        const auto* s=store_.get(k);
        if(s&&!s->events.empty())first=std::min(first,s->events.back().event_ns);
      }
      for(const auto& p:d.dependencies){
        const size_t j=index.at(p);
        children[j].push_back(i);++degree[i];
        if(due[j]!=INF)due[i]=std::min(due[i],due[j]);
        first=std::min(first,earliest[j]);
      }
      earliest[i]=first;
      if(first!=INF && d.max_source_age_ns>0)
        due[i]=std::min(due[i],d.max_source_age_ns>INF-first?INF:first+d.max_source_age_ns);
    }
    std::vector<int64_t> priority_due=due;
    for(size_t pos=n;pos-->0;){
      critical[pos]=cost_of(ordered[pos]);
      for(size_t child:children[pos]){
        priority_due[pos]=std::min(priority_due[pos],priority_due[child]);
        const uint64_t tail=critical[child];
        const auto own=cost_of(ordered[pos]);
        critical[pos]=std::max(critical[pos],tail>UINT64_MAX-own?
                                 UINT64_MAX:tail+own);
      }
    }
    struct Ready{long double slack;std::string id;size_t index;};
    auto later=[](const Ready& a,const Ready& b){
      if(a.slack!=b.slack)return a.slack>b.slack;
      return a.id>b.id;
    };
    std::priority_queue<Ready,std::vector<Ready>,decltype(later)> queue(later);
    auto add=[&](size_t i){queue.push({static_cast<long double>(priority_due[i])-static_cast<long double>(now_ns)-
                      static_cast<long double>(critical[i]),ordered[i].id,i});};
    for(size_t i=0;i<n;++i)if(degree[i]==0)add(i);
    uint64_t remaining=options.compute_budget_ns;
    size_t emitted=0;
    while(!queue.empty()){
      const size_t i=queue.top().index;queue.pop();
      const Descriptor& d=ordered[i];
      const uint64_t charge=cost_of(d);
      bool allow=(options.compute_budget_ns==0 || charge<=remaining);
      auto out=evaluate(d,outputs,allow,true);
      if(out.computed && options.compute_budget_ns!=0)remaining-=charge;
      publish(d,std::move(out));++emitted;
      for(size_t child:children[i])if(--degree[child]==0)add(child);
    }
    if(emitted!=n)throw std::logic_error("validated dependency graph unexpectedly incomplete");
    finalize();return outputs;
  }
  for(const auto& d:ordered){
    const bool allow=(policy!=Policy::FixedCadence || d.cadence==1 || !memo_.contains(d.id) ||
                      cycles_==1 || (cycles_-1)%d.cadence==0);
    publish(d,evaluate(d,outputs,allow,cache_policy));
  }
  finalize();return outputs;
}
void Session::reset(){store_.reset();memo_.clear();hits_=computations_=cycles_=0;last_eval_ns_=0;last_run_={};measured_cost_ns_.clear();}
std::string name(Status s){switch(s){case Status::Valid:return "VALID";case Status::Stale:return "STALE";case Status::Unavailable:return "UNAVAILABLE";case Status::Failed:return "FAILED";}return "UNKNOWN";}
std::string name(Mode s){switch(s){case Mode::Observed:return "OBSERVED";case Mode::Estimated:return "ESTIMATED";case Mode::Simulated:return "SIMULATED";}return "UNKNOWN";}
std::string encode_event(const Event& e){
  std::ostringstream ss;ss.imbue(std::locale::classic());ss<<std::setprecision(std::numeric_limits<double>::max_digits10);
  ss<<"MA1|"<<(e.key.kind==Kind::Bar?'B':'L')<<'|'<<e.key.source<<'|'<<e.key.symbol<<'|'<<e.key.timeframe<<'|'<<e.seq<<'|'<<e.event_ns<<'|'<<e.ingest_ns<<'|'<<e.a<<'|'<<e.b<<'|'<<e.c<<'|'<<e.d<<'|'<<e.e<<'|'<<static_cast<int>(e.mode);return ss.str();
}
Event decode_event(const std::string& line){
  const auto p=parts(line);if(p.size()!=14||p[0]!="MA1"||(p[1]!="B"&&p[1]!="L"))throw std::invalid_argument("invalid trace record");
  for(size_t i=2;i<=4;++i)if(p[i].empty()||p[i].find('|')!=std::string::npos)throw std::invalid_argument("invalid source identity");
  Event e;e.key={p[2],p[3],p[4],p[1]=="B"?Kind::Bar:Kind::Book};
  e.seq=parse_u(p[5]);e.event_ns=parse_i(p[6]);e.ingest_ns=parse_i(p[7]);
  e.a=parse_d(p[8]);e.b=parse_d(p[9]);e.c=parse_d(p[10]);e.d=parse_d(p[11]);e.e=parse_d(p[12]);
  const auto m=parse_i(p[13]);if(m<0||m>2)throw std::invalid_argument("invalid mode");e.mode=static_cast<Mode>(m);return e;
}
std::string normalized_result(const Results& result){
  std::ostringstream ss;ss.imbue(std::locale::classic());ss<<std::setprecision(std::numeric_limits<double>::max_digits10);
  for(const auto& [id,r]:result){ss<<id<<'|'<<name(r.status)<<'|'<<name(r.mode)<<'|';if(r.value)ss<<*r.value;
    ss<<'|'<<r.source_ns<<'|'<<r.identity<<'|'<<r.reason<<'|'<<r.last_known_identity<<'|';
    if(r.last_known_value)ss<<*r.last_known_value;
    ss<<'|'<<r.last_known_source_ns<<'|'<<r.last_known_computed_ns<<'\n';
  }
  return ss.str();
}
}
