#include "metalarch/core.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <queue>
#include <set>
#include <sstream>
#include <utility>
namespace ma {
namespace {
constexpr uint64_t OFFSET=14695981039346656037ULL, PRIME=1099511628211ULL;
void mix(uint64_t& h, uint64_t v) { for (int n=0;n<8;++n) {h ^= (v & 255U); h *= PRIME; v >>= 8U;} }
void mix(uint64_t& h, const std::string& s) { mix(h, s.size()); for(unsigned char c:s){h^=c;h*=PRIME;} }
std::vector<std::string> parts(const std::string& s) {
  std::vector<std::string> p; size_t start=0;
  for(size_t pos=0;pos<=s.size();++pos) if(pos==s.size()||s[pos]=='|') {p.push_back(s.substr(start,pos-start));start=pos+1;}
  return p;
}
int64_t parse_i(const std::string& s) {size_t end=0;auto v=std::stoll(s,&end);if(end!=s.size())throw std::invalid_argument("invalid integer");return v;}
uint64_t parse_u(const std::string& s) {size_t end=0;auto v=std::stoull(s,&end);if(end!=s.size()||s[0]=='-')throw std::invalid_argument("invalid unsigned");return v;}
double parse_d(const std::string& s) {size_t end=0;auto v=std::stod(s,&end);if(end!=s.size()||!std::isfinite(v))throw std::invalid_argument("invalid float");return v;}
}
Ingest Store::ingest(const Event& e) {
  if(e.key.source.empty() || e.key.symbol.empty() || e.key.timeframe.empty() ||
     e.key.source.find_first_of("|\r\n")!=std::string::npos ||
     e.key.symbol.find_first_of("|\r\n")!=std::string::npos ||
     e.key.timeframe.find_first_of("|\r\n")!=std::string::npos ||
     e.seq==0 || e.event_ns<=0 || e.ingest_ns<=0 || e.event_ns>e.ingest_ns ||
     !std::isfinite(e.a)||!std::isfinite(e.b)||!std::isfinite(e.c)||!std::isfinite(e.d)||!std::isfinite(e.e) ||
     (e.key.kind==Kind::Bar && (e.a<=0||e.b<e.a||e.b<e.d||e.c>e.a||e.c>e.d||e.c<=0||e.d<=0||e.e<0)) ||
     (e.key.kind==Kind::Book && (e.a<=0||e.b<e.a||e.c<0||e.d<0))) return Ingest::Invalid;
  auto& s=streams_[e.key];
  if(!s.events.empty()){
    const Event& prev=s.events.back();
    if(e.seq==prev.seq){
      // Exact duplicate is idempotent; divergent same-sequence event is rejected.
      return encode_event(e)==encode_event(prev)?Ingest::Duplicate:Ingest::Invalid;
    }
    if(e.seq<prev.seq || e.event_ns<prev.event_ns) return Ingest::OutOfOrder;
  }
  s.events.push_back(e);
  if(s.events.size()>max_events_) s.events.erase(s.events.begin());
  ++s.version;
  // Rolling source-content digest: version alone is not enough to identify traces.
  const auto encoded=encode_event(e);
  for(unsigned char ch:encoded){s.digest^=ch;s.digest*=PRIME;}
  return Ingest::Accepted;
}
const Stream* Store::get(const Key& k) const {auto it=streams_.find(k);return it==streams_.end()?nullptr:&it->second;}
uint64_t Store::version(const Key& k) const {auto s=get(k);return s?s->version:0;}
Graph::Graph(std::vector<Descriptor> specs) {
  std::map<std::string,Descriptor> by_id;
  for(auto& d:specs){
    if(d.id.empty()||d.revision.empty()||!d.compute||d.max_source_age_ns<0||!by_id.emplace(d.id,std::move(d)).second)
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
  uint64_t h=OFFSET;mix(h,d.id);mix(h,d.revision);mix(h,d.parameters);mix(h,static_cast<uint64_t>(d.max_source_age_ns));
  for(const auto& k:d.sources){mix(h,k.source);mix(h,k.symbol);mix(h,k.timeframe);mix(h,static_cast<uint64_t>(k.kind));mix(h,s.version(k));
    if(const auto* stream=s.get(k))mix(h,stream->digest);}
  for(const auto& id:d.dependencies){
    const auto it=parents.find(id);if(it==parents.end())throw std::logic_error("parent was not executed");
    mix(h,id);mix(h,it->second.identity);mix(h,static_cast<uint64_t>(it->second.status));
    mix(h,static_cast<uint64_t>(it->second.mode));
  }
  return h;
}
Results Session::execute(int64_t now_ns,bool cache_enabled){
  if(now_ns<=0)throw std::invalid_argument("invalid evaluation timestamp");
  Results outputs;
  for(const auto& d:graph_.sorted()){
    bool blocked=false;std::string why;
    int64_t source_ns=std::numeric_limits<int64_t>::max();
    Mode mode=Mode::Observed;
    std::vector<std::string> lineage;
    for(const auto& k:d.sources){
      const auto* stream=store_.get(k);
      if(!stream||stream->events.empty()){blocked=true;why="source unavailable";break;}
      const auto& ev=stream->events.back();
      if(ev.event_ns>now_ns){blocked=true;why="source from future";break;}
      source_ns=std::min(source_ns,ev.event_ns);
      mode=std::max(mode,ev.mode);
      lineage.push_back(k.source+":"+k.symbol+":"+k.timeframe+":"+std::to_string(stream->version)+":"+std::to_string(stream->digest));
    }
    for(const auto& parent:d.dependencies){
      const auto& p=outputs.at(parent);
      if(p.status!=Status::Valid){blocked=true;why="upstream "+parent+" is "+name(p.status);break;}
      source_ns=std::min(source_ns,p.source_ns);
      mode=std::max(mode,p.mode);
      lineage.insert(lineage.end(),p.lineage.begin(),p.lineage.end());
    }
    const uint64_t signature=fingerprint(d,store_,outputs);
    Result r;
    if(blocked){r.status=Status::Unavailable;r.reason=why;}
    else if(d.max_source_age_ns>0 && now_ns-source_ns>d.max_source_age_ns){
      r.status=Status::Stale;r.reason="source age exceeds limit";
    }else{
      auto it=memo_.find(d.id);
      if(cache_enabled&&it!=memo_.end()&&it->second.identity==signature){r=it->second;++hits_;}
      else{
        try{
          ReadView view(store_,d.sources);Results parent_inputs;
          for(const auto& parent:d.dependencies)parent_inputs.emplace(parent,outputs.at(parent));
          r=d.compute(view,parent_inputs);++computations_;
        }
        catch(const std::exception& e){r=Result{};r.status=Status::Failed;r.reason=e.what();++computations_;}
        catch(...){r=Result{};r.status=Status::Failed;r.reason="unknown engine exception";++computations_;}
        if(r.status==Status::Valid && (!r.value || !std::isfinite(*r.value))){r.status=Status::Failed;r.value.reset();r.reason="valid result has no finite value";}
        if(r.status==Status::Valid && cache_enabled) memo_[d.id]=r;
      }
    }
    r.identity=signature;r.mode=mode;r.lineage=std::move(lineage);
    r.source_ns=source_ns==std::numeric_limits<int64_t>::max()?0:source_ns;
    // Logical time is assigned on publication; wall-clock profiling is collected elsewhere.
    if(r.computed_ns==0)r.computed_ns=now_ns;
    if(r.status!=Status::Valid) r.value.reset();
    if(r.status==Status::Valid&&cache_enabled){memo_[d.id]=r;}
    outputs.emplace(d.id,std::move(r));
  }
  return outputs;
}
void Session::reset(){store_.reset();memo_.clear();hits_=computations_=0;}
std::string name(Status s){switch(s){case Status::Valid:return "VALID";case Status::Stale:return "STALE";case Status::Unavailable:return "UNAVAILABLE";case Status::Failed:return "FAILED";}return "UNKNOWN";}
std::string name(Mode s){switch(s){case Mode::Observed:return "OBSERVED";case Mode::Estimated:return "ESTIMATED";case Mode::Simulated:return "SIMULATED";}return "UNKNOWN";}
std::string encode_event(const Event& e){
  std::ostringstream ss;ss<<std::setprecision(std::numeric_limits<double>::max_digits10);
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
  std::ostringstream ss;ss<<std::setprecision(std::numeric_limits<double>::max_digits10);
  for(const auto& [id,r]:result){ss<<id<<'|'<<name(r.status)<<'|'<<name(r.mode)<<'|';if(r.value)ss<<*r.value;
    ss<<'|'<<r.source_ns<<'|'<<r.identity<<'|'<<r.reason<<'\n';}
  return ss.str();
}
}