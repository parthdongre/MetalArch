#include "metalarch/engines.hpp"
#include "metalarch/resource.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ma;
namespace {
Policy policy_of(const std::string& name){
 if(name=="b0")return Policy::SequentialFull;
 if(name=="b1")return Policy::ParallelFull;
 if(name=="b2")return Policy::Cache;
 if(name=="b3")return Policy::FixedCadence;
 if(name=="p")return Policy::Freshness;
 throw std::invalid_argument("unknown profiling policy");
}
}
int main(int argc,char** argv){
 try{
  if(argc<6||argc>7){
   std::cerr<<"usage: metalarch_profile_session <input.ma1> <output.csv> <core|expanded> <b0|b1|b2|b3|p> <cost_budget_ns> [frozen_calibration.tsv]\n";
   return 2;
  }
  const std::string cohort=argv[3];if(cohort!="core" && cohort!="expanded")throw std::invalid_argument("invalid cohort");
  const auto policy=policy_of(argv[4]);
  const auto budget=std::stoull(argv[5]);if(budget==0)throw std::invalid_argument("use a positive nominal cost budget");
  if(argc==7 && policy!=Policy::Freshness)throw std::invalid_argument("frozen cost table only applies to P");
  const Key gold{"fixture","XAU","1m",Kind::Bar},silver{"fixture","XAG","1m",Kind::Bar},book{"fixture","XAU","live",Kind::Book};
  Graph graph=make_metal_graph(gold,silver,book,cohort=="expanded");
  Session session(graph);
  PolicyOptions options{4,budget};options.profile_resources=true;
  if(argc==7){session.set_frozen_costs(load_frozen_cost_table(argv[6],graph));options.cost_model=CostModel::FrozenCalibration;}
  std::ifstream trace(argv[1]);if(!trace)throw std::runtime_error("cannot read profile trace");
  std::ofstream csv(argv[2],std::ios::trunc);if(!csv)throw std::runtime_error("cannot open profile CSV");
  csv<<"event_index,wall_us,process_cpu_ns,rss_after_bytes,process_peak_rss_bytes,reserved_event_payload_bytes_lower_bound,compute_wall_sum_ns,thread_cpu_sum_ns,compute_count,cache_hits,deferred,worker_queue_highwater,valid,stale,unavailable,failed\n";
  csv<<std::fixed<<std::setprecision(6);
  uint64_t i=0,overall_computes=0,overall_hits=0,overall_deferrals=0;
  int64_t last_ingest=0;std::string line;
  ResourceSnapshot latest{};
  uint64_t reserved_bytes=0;
  while(std::getline(trace,line)){
   if(line.empty()||line[0]=='#')continue;
   const auto e=decode_event(line);
   if(e.mode!=Mode::Simulated||e.key.source!="fixture"||e.ingest_ns<last_ingest)
     throw std::invalid_argument("session profiler requires globally ordered SIMULATED fixture input");
   last_ingest=e.ingest_ns;
   const auto started=std::chrono::steady_clock::now();
   if(session.store().ingest(e)!=Ingest::Accepted)throw std::runtime_error("rejected profile input");
   const auto r=session.execute(e.ingest_ns,policy,options);
   const double wall_us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count();
   const auto& s=session.last_run();
   if(!s.resources)throw std::logic_error("resource profiling unexpectedly disabled");
   const auto& resource=*s.resources;
   latest=sample_process_resources();
   reserved_bytes=resource.reserved_event_payload_bytes_lower_bound;
   overall_computes+=s.computations;overall_hits+=s.cache_hits;overall_deferrals+=s.deferred;
   uint64_t valid=0,stale=0,unavailable=0,failed=0;
   for(const auto& [name,v]:r){(void)name;switch(v.status){case Status::Valid:++valid;break;case Status::Stale:++stale;break;case Status::Unavailable:++unavailable;break;case Status::Failed:++failed;break;}}
   csv<<++i<<','<<wall_us<<','<<resource.process_cpu_ns<<',';
   if(resource.current_rss_supported)csv<<resource.rss_after_bytes;
   csv<<',';
   if(resource.peak_rss_supported)csv<<resource.process_peak_rss_bytes;
   csv<<','<<reserved_bytes<<','<<s.actual_compute_wall_sum_ns<<','<<s.actual_compute_thread_cpu_sum_ns
      <<','<<s.computations<<','<<s.cache_hits<<','<<s.deferred<<','<<s.worker_queue_highwater
      <<','<<valid<<','<<stale<<','<<unavailable<<','<<failed<<'\n';
  }
  if(!csv)throw std::runtime_error("profile CSV write failed");
  std::cout<<"profile_events="<<i<<" policy="<<argv[4]<<" cohort="<<cohort
           <<" computed="<<overall_computes<<" hits="<<overall_hits<<" deferred="<<overall_deferrals
           <<" reserved_event_storage_lower_bound_bytes="<<reserved_bytes<<" process_peak_rss_bytes=";
  if(latest.peak_rss_supported)std::cout<<latest.peak_rss_bytes;else std::cout<<"UNAVAILABLE";
  std::cout<<" process_current_rss_bytes=";
  if(latest.current_rss_supported)std::cout<<latest.current_rss_bytes;else std::cout<<"UNAVAILABLE";
  std::cout<<'\n';
  return 0;
 }catch(const std::exception& e){std::cerr<<"single-session profile ERROR: "<<e.what()<<'\n';return 1;}
}
