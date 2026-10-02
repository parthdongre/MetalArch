#include "metalarch/engines.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
using namespace ma;
namespace {
struct Sample {
  double wall_us{0}, cpu_us{0};
  uint64_t computes{0}, hits{0}, deferred{0}, estimated_ns{0};
  int valid{0}, stale{0}, missing{0}, failed{0}, mismatches{0}, unflagged_age{0};
  double largest_error{0};
};
std::string policy_name(Policy p) {
  switch(p){
    case Policy::SequentialFull:return "B0_full";
    case Policy::ParallelFull:return "B1_parallel";
    case Policy::Cache:return "B2_cache";
    case Policy::FixedCadence:return "B3_cadence";
    case Policy::Freshness:return "P_freshness";
  }
  throw std::logic_error("invalid policy");
}
std::vector<Event> load(const std::string& path){
  std::ifstream in(path);
  if(!in)throw std::runtime_error("cannot open MA1 input trace");
  std::vector<Event> data;
  std::string line;
  int64_t last_ingest=0;
  while(std::getline(in,line)){
    if(line.empty() || line[0]=='#')continue;
    auto e=decode_event(line);
    // This initial benchmark's graph is deliberately fixed to simulated
    // fixture streams. Arbitrary external traces require a separate adapter.
    const bool supported=(e.key.source=="fixture" && e.mode==Mode::Simulated &&
       ((e.key.symbol=="XAU"&&e.key.timeframe=="1m"&&e.key.kind==Kind::Bar)||
        (e.key.symbol=="XAG"&&e.key.timeframe=="1m"&&e.key.kind==Kind::Bar)||
        (e.key.symbol=="XAU"&&e.key.timeframe=="live"&&e.key.kind==Kind::Book)));
    if(!supported)throw std::runtime_error("benchmark supports only specified SIMULATED fixture streams");
    if(e.ingest_ns<last_ingest)throw std::runtime_error("trace must be globally ordered by ingestion time");
    last_ingest=e.ingest_ns;
    data.push_back(std::move(e));
  }
  if(data.empty())throw std::runtime_error("trace is empty");
  return data;
}
bool same_current_semantics(const Results& actual,const Results& oracle){
  if(actual.size()!=oracle.size())return false;
  for(const auto& [id,a]:actual){
    auto it=oracle.find(id);if(it==oracle.end())return false;
    const auto& b=it->second;
    if(a.status!=b.status || a.mode!=b.mode || a.value!=b.value ||
       a.source_ns!=b.source_ns || a.identity!=b.identity || a.reason!=b.reason ||
       a.lineage!=b.lineage)return false;
    // Display-only last_known_value and previous computed-at time can differ
    // legitimately for B0 (no cache) vs B2 (cache); never compare as fresh data.
  }
  return true;
}
double percentile(std::vector<double> x,double q){
  if(x.empty())return 0;
  std::sort(x.begin(),x.end());
  const auto position=(static_cast<double>(x.size())-1)*q;
  auto lo=static_cast<size_t>(position);
  auto hi=std::min(lo+1,x.size()-1);
  return x[lo]+(x[hi]-x[lo])*(position-static_cast<double>(lo));
}
Sample inspect(const Results& got,const Results& oracle,const Graph& graph){
  Sample result;
  for(const auto& d:graph.sorted()){
    const auto& g=got.at(d.id),o=oracle.at(d.id);
    switch(g.status){
      case Status::Valid:++result.valid;break;
      case Status::Stale:++result.stale;break;
      case Status::Unavailable:++result.missing;break;
      case Status::Failed:++result.failed;break;
    }
    if(g.status!=o.status)++result.mismatches;
    if(g.status==Status::Valid && o.status==Status::Valid){
      if(!g.value || !o.value)throw std::runtime_error("valid value missing");
      result.largest_error=std::max(result.largest_error,std::abs(*g.value-*o.value));
    }
    if(g.status==Status::Valid && d.max_source_age_ns>0 && g.source_ns>0 &&
       g.computed_ns-g.source_ns>d.max_source_age_ns)++result.unflagged_age;
    if(g.status!=Status::Valid && g.value)throw std::runtime_error("invalid output presents fresh numeric value");
  }
  return result;
}
}
int main(int argc,char** argv){
 try{
  if(argc<3||argc>7){
    std::cerr<<"usage: metalarch_compare <input.ma1> <raw.csv> [runs=10] [warmup_bars=65] [P_budget_ns=8000000] [cohort=expanded|core]\n";
    return 2;
  }
  const auto trace=load(argv[1]);
  const auto runs=(argc>=4?std::stoi(argv[3]):10);
  const auto warmup_bars=(argc>=5?std::stoi(argv[4]):65);
  const auto budget=(argc>=6?std::stoull(argv[5]):8'000'000ULL);
  if(runs<1||runs>100||warmup_bars<0||warmup_bars*3>=static_cast<int>(trace.size())||budget==0)
    throw std::invalid_argument("invalid runs/warmup/budget");
  const Key gold{"fixture","XAU","1m",Kind::Bar},silver{"fixture","XAG","1m",Kind::Bar},book{"fixture","XAU","live",Kind::Book};
  const std::string cohort=(argc>=7?argv[6]:"expanded");
  if(cohort!="expanded" && cohort!="core")throw std::invalid_argument("cohort must be expanded or core");
  const Graph graph=make_metal_graph(gold,silver,book,cohort=="expanded");
  // Reference is generated independently and never shared with candidate state.
  Session oracle(graph);
  std::vector<Results> reference;
  reference.reserve(trace.size());
  for(const auto& e:trace){
    if(oracle.store().ingest(e)!=Ingest::Accepted)throw std::runtime_error("non-unique or invalid benchmark event");
    reference.push_back(oracle.execute(e.ingest_ns,Policy::SequentialFull));
  }
  std::ofstream raw(argv[2]);
  if(!raw)throw std::runtime_error("cannot create raw output file");
  raw.imbue(std::locale::classic());
  raw<<"run,policy,event_index,ingest_ns,wall_us,process_cpu_us,computations,hits,deferred,estimated_cost_ns,valid,stale,unavailable,failed,status_mismatches,max_common_valid_abs_error,unflagged_age_violations\n";
  raw<<std::fixed<<std::setprecision(6);
  std::vector<Policy> policies={Policy::SequentialFull,Policy::ParallelFull,Policy::Cache,Policy::FixedCadence,Policy::Freshness};
  for(int run=0;run<runs;++run){
    // Alternated trial order to reduce systematic warmed-cache/thermal bias.
    std::rotate(policies.begin(),policies.begin()+1,policies.end());
    for(auto policy:policies){
      Session session(graph);
      std::vector<double> timings;
      int mismatch=0,unflagged=0,valid=0,stale=0,failed=0,missing=0;
      uint64_t computes=0,hits=0,deferrals=0;
      double maxerror=0;
      for(size_t i=0;i<trace.size();++i){
        const auto& e=trace[i];
        const auto wall_start=std::chrono::steady_clock::now();
        const auto cpu_start=std::clock();
        if(session.store().ingest(e)!=Ingest::Accepted)throw std::runtime_error("benchmark input rejected");
        const auto result=session.execute(e.ingest_ns,policy,{4,budget});
        const auto wall_end=std::chrono::steady_clock::now();
        const auto cpu_end=std::clock();
        if(!same_current_semantics(result,reference[i]) &&
           (policy==Policy::SequentialFull||policy==Policy::ParallelFull||policy==Policy::Cache))
          throw std::runtime_error("correct baseline diverges at policy="+policy_name(policy)+" input index="+std::to_string(i)+"\nCURRENT\n"+normalized_result(result)+"REFERENCE\n"+normalized_result(reference[i]));
        if(i<static_cast<size_t>(warmup_bars)*3)continue;
        Sample v=inspect(result,reference[i],graph);
        v.wall_us=std::chrono::duration<double,std::micro>(wall_end-wall_start).count();
        v.cpu_us=1e6*static_cast<double>(cpu_end-cpu_start)/CLOCKS_PER_SEC;
        v.computes=session.last_run().computations;
        v.hits=session.last_run().cache_hits;
        v.deferred=session.last_run().deferred;
        v.estimated_ns=session.last_run().estimated_cost_ns;
        timings.push_back(v.wall_us);
        mismatch+=v.mismatches;unflagged+=v.unflagged_age;
        valid+=v.valid;stale+=v.stale;missing+=v.missing;failed+=v.failed;
        computes+=v.computes;hits+=v.hits;deferrals+=v.deferred;
        maxerror=std::max(maxerror,v.largest_error);
        raw<<run<<','<<policy_name(policy)<<','<<i<<','<<e.ingest_ns<<','<<v.wall_us<<','<<v.cpu_us<<','<<v.computes<<','<<v.hits<<','<<v.deferred<<','<<v.estimated_ns<<','<<v.valid<<','<<v.stale<<','<<v.missing<<','<<v.failed<<','<<v.mismatches<<','<<v.largest_error<<','<<v.unflagged_age<<'\n';
      }
      if(unflagged)throw std::runtime_error("VALID output violates source age in "+policy_name(policy));
      std::cout<<"run="<<run<<" policy="<<policy_name(policy)<<" samples="<<timings.size()
               <<" p50_us="<<std::fixed<<std::setprecision(3)<<percentile(timings,.5)
               <<" p95_us="<<percentile(timings,.95)<<" p99_us="<<percentile(timings,.99)
               <<" computes="<<computes<<" hits="<<hits<<" deferred="<<deferrals
               <<" valid="<<valid<<" stale="<<stale<<" unavailable="<<missing
               <<" failed="<<failed<<" status_mismatches="<<mismatch<<" max_common_error="<<maxerror<<'\n';
    }
  }
  if(!raw)throw std::runtime_error("raw output write failure");
  return 0;
 }catch(const std::exception& e){std::cerr<<"benchmark ERROR: "<<e.what()<<'\n';return 1;}
}
