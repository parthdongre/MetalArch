#include "metalarch/engines.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
using namespace ma;
namespace {
uint64_t quantile(std::vector<uint64_t> values,double q){
 if(values.empty())return 0;
 std::sort(values.begin(),values.end());
 return values[static_cast<size_t>(std::ceil(q*static_cast<double>(values.size())))-1];
}
struct Observations{
 std::vector<uint64_t> wall_ns,thread_cpu_ns;
};
}
int main(int argc,char** argv){
 try{
  if(argc<3||argc>6){
   std::cerr<<"usage: metalarch_calibrate <PRIOR_INPUT_TRACE.ma1> <out.tsv> [core|expanded] [warmup_events=195] [min_samples=5]\n";
   return 2;
  }
  const std::string cohort=argc>3?argv[3]:"expanded";
  if(cohort!="expanded" && cohort!="core")throw std::invalid_argument("unknown cohort");
  const auto warmup=(argc>4?std::stoull(argv[4]):195ULL);
  const auto minimum=(argc>5?std::stoull(argv[5]):5ULL);
  if(minimum<5)throw std::invalid_argument("at least five valid calibration samples per engine required");
  const Key gold{"fixture","XAU","1m",Kind::Bar},silver{"fixture","XAG","1m",Kind::Bar},book{"fixture","XAU","live",Kind::Book};
  Graph graph=make_metal_graph(gold,silver,book,cohort=="expanded");
  Session session(graph);
  std::ifstream in(argv[1]);if(!in)throw std::runtime_error("cannot open calibration trace");
  std::map<std::string,Observations> observations;
  for(const auto& d:graph.sorted())observations.try_emplace(d.id);
  std::string line;uint64_t events=0;int64_t last_ingest=0;
  while(std::getline(in,line)){
   if(line.empty()||line[0]=='#')continue;
   const auto e=decode_event(line);
   if(e.mode!=Mode::Simulated||e.key.source!="fixture"||e.ingest_ns<last_ingest)
     throw std::invalid_argument("calibration requires globally ordered SIMULATED fixture input");
   last_ingest=e.ingest_ns;
   if(session.store().ingest(e)!=Ingest::Accepted)throw std::invalid_argument("duplicate/invalid calibration event");
   const auto results=session.execute(e.ingest_ns,Policy::SequentialFull,{1,0,50'000,CostModel::Declared,true});
   if(++events<=warmup)continue;
   for(const auto& sample:session.last_run().engine_measurements){
    if(sample.status!=Status::Valid)continue;
    auto& s=observations.at(sample.id);
    s.wall_ns.push_back(std::max<uint64_t>(1,sample.wall_ns));
    if(sample.thread_cpu_ns)s.thread_cpu_ns.push_back(*sample.thread_cpu_ns);
   }
   (void)results;
  }
  // Validate completeness before creating the table file (no partial tables).
  for(const auto& [name,s]:observations)
   if(s.wall_ns.size()<minimum)throw std::runtime_error("not enough post-warmup valid samples for: "+name);
  std::ofstream out(argv[2],std::ios::trunc);if(!out)throw std::runtime_error("cannot write calibration table");
  out<<"id\trevision\tparameters\tdescriptor_digest\tvalid_samples\twall_p50_ns\twall_p95_ns\tthread_cpu_p95_ns\tcost_ns\n";
  for(const auto& d:graph.sorted()){
   const auto& s=observations.at(d.id);
   const uint64_t p50=quantile(s.wall_ns,.50),p95=quantile(s.wall_ns,.95),cpu=quantile(s.thread_cpu_ns,.95);
   if(p95>UINT64_MAX-p95/5)throw std::overflow_error("calibrated cost overflow");
   const uint64_t cost=std::max<uint64_t>(1'000,p95+p95/5); // 20% admission guard, NOT an enforced deadline
   out<<d.id<<'\t'<<d.revision<<'\t'<<d.parameters<<'\t'<<descriptor_signature(d)<<'\t'<<s.wall_ns.size()<<'\t'
      <<p50<<'\t'<<p95<<'\t'<<cpu<<'\t'<<cost<<'\n';
  }
  if(!out)throw std::runtime_error("calibration output failed");
  std::cerr<<"calibration_input_events="<<events<<" warmup="<<warmup<<" cohort="<<cohort
           <<" profile=enabled cpu=0_if_unsupported policy=uncached_B0\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"calibration error: "<<e.what()<<'\n';return 1;}
}
