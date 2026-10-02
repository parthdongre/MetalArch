#include "metalarch/engines.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
ma::Policy parse_policy(const std::string& s){
  if(s=="b0")return ma::Policy::SequentialFull;
  if(s=="b1")return ma::Policy::ParallelFull;
  if(s=="b2")return ma::Policy::Cache;
  if(s=="b3")return ma::Policy::FixedCadence;
  if(s=="p")return ma::Policy::Freshness;
  throw std::invalid_argument("policy must be b0, b1, b2, b3 or p");
}
}
int main(int argc,char** argv){
 if(argc<3||argc>5||std::string(argv[1])!="replay"){
   std::cerr<<"usage: metalarch_cli replay <trace.ma1> [b0|b1|b2|b3|p] [p_budget_nominal_ns]\n";
   return 2;
 }
 const ma::Key gold{"fixture","XAU","1m",ma::Kind::Bar},silver{"fixture","XAG","1m",ma::Kind::Bar},book{"fixture","XAU","live",ma::Kind::Book};
 ma::Session session(ma::make_metal_graph(gold,silver,book));
 std::ifstream input(argv[2]);if(!input){std::cerr<<"cannot read trace\n";return 2;}
 size_t accepted=0,dups=0;uint64_t hits=0,computes=0,deferred=0;
 try{
   const auto policy=(argc>=4?parse_policy(argv[3]):ma::Policy::Cache);
   uint64_t budget=8'000'000;
   if(argc>=5){size_t end=0;const std::string v(argv[4]);budget=std::stoull(v,&end);
     if(end!=v.size()||budget==0)throw std::invalid_argument("budget must be positive");}
   std::string line;
   int64_t previous_ingest_ns=0;
   while(std::getline(input,line)){
     if(line.empty()||line[0]=='#')continue;
     const auto e=ma::decode_event(line);
     if(e.ingest_ns<previous_ingest_ns)throw std::runtime_error("global ingestion times must be nondecreasing");
     previous_ingest_ns=e.ingest_ns;
     const auto status=session.store().ingest(e);
     if(status==ma::Ingest::Accepted){
       ++accepted;
       auto result=session.execute(e.ingest_ns,policy,{4,budget});
       std::cout<<"event="<<accepted<<'|'<<ma::normalized_result(result);
       hits+=session.last_run().cache_hits;computes+=session.last_run().computations;
       deferred+=session.last_run().deferred;
     }
     else if(status==ma::Ingest::Duplicate)++dups;
     else throw std::runtime_error("invalid or out-of-order source event");
   }
 }catch(const std::exception& e){std::cerr<<"trace error: "<<e.what()<<'\n';return 1;}
 std::cerr<<"accepted="<<accepted<<" duplicates="<<dups<<" cache_hits="<<hits
          <<" computes="<<computes<<" deferred="<<deferred<<'\n';
 return 0;
}
