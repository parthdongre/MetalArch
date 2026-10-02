#include "metalarch/engines.hpp"
#include "metalarch/trace.hpp"
#include <vector>
#include <set>
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
 if(argc>=2 && std::string(argv[1])=="inventory"){
   if(argc>3){std::cerr<<"usage: metalarch_cli inventory [core|expanded|core-inc|expanded-inc]\n";return 2;}
   const std::string cohort=argc==3?argv[2]:"expanded";
   if(cohort!="core" && cohort!="expanded" && cohort!="core-inc" && cohort!="expanded-inc"){
     std::cerr<<"cohort must be core, expanded, core-inc or expanded-inc\n";return 2;
   }
   const ma::Key gold{"fixture","XAU","1m",ma::Kind::Bar}, silver{"fixture","XAG","1m",ma::Kind::Bar},
                 book{"fixture","XAU","live",ma::Kind::Book};
   const auto graph=ma::make_metal_graph(gold,silver,book,cohort=="expanded"||cohort=="expanded-inc",
                                        cohort=="core-inc"||cohort=="expanded-inc");
   std::cout<<"id\trevision\tparameters\tinputs\tparents\tmax_source_age_ns\tmax_result_age_ns\tcadence\tdeclared_cost_ns\n";
   for(const auto& d:graph.sorted()){
     std::string sources,parents;
     for(const auto& k:d.sources){if(!sources.empty())sources+=",";
       sources+=k.source+":"+k.symbol+":"+k.timeframe+":"+(k.kind==ma::Kind::Bar?"bar":"book");}
     for(const auto& dep:d.dependencies){if(!parents.empty())parents+=",";parents+=dep;}
     std::cout<<d.id<<'\t'<<d.revision<<'\t'<<d.parameters<<'\t'<<sources<<'\t'<<parents<<'\t'
              <<d.max_source_age_ns<<'\t'<<d.max_result_age_ns<<'\t'<<d.cadence<<'\t'<<d.estimated_cost_ns<<'\n';
   }
   return 0;
 }
 if(argc<3||argc>8||std::string(argv[1])!="replay"){
   std::cerr<<"usage: metalarch_cli replay <trace.ma1> [b0|b1|b2|b3|p] [budget_cost_ns] [cohort=expanded|core] [FROZEN_COSTS.tsv|-] [EXPECTED_MA2_ROOT]\n";
   return 2;
 }
 ma::Key gold{"fixture","XAU","1m",ma::Kind::Bar},silver{"fixture","XAG","1m",ma::Kind::Bar},book{"fixture","XAU","live",ma::Kind::Book};
 size_t accepted=0,dups=0;uint64_t hits=0,computes=0,deferred=0;
 // MA2 internal integrity is checked before results; an externally pinned root is optional.
 // Discover the actual source keys; don't rebrand user-provided data as the simulated fixture.
 bool is_ma2=false;
 std::string preverified_root;
 try{
   std::ifstream detect(argv[2],std::ios::binary);
   if(!detect)throw std::runtime_error("cannot read trace");
   std::string first;std::getline(detect,first);
   is_ma2=first.rfind("MA2|",0)==0;
   if(is_ma2){
     ma::TraceReader inspect(argv[2]);ma::Event event;
     std::vector<ma::Key> bars,books;std::set<ma::Key> seen;
     while(inspect.next(event))if(seen.insert(event.key).second){
       if(event.key.kind==ma::Kind::Bar)bars.push_back(event.key);
       else books.push_back(event.key);
     }
     if(bars.empty())throw std::invalid_argument("MA2 terminal requires at least one bar stream");
     gold=bars.front();
     for(const auto& k:bars)if(k.symbol=="XAU"){gold=k;break;}
     silver={gold.source,"__UNAVAILABLE_PEER__",gold.timeframe,ma::Kind::Bar};
     for(const auto& k:bars)if(k.symbol!=gold.symbol && k.timeframe==gold.timeframe){silver=k;break;}
     book={gold.source,"__UNAVAILABLE_BOOK__","live",ma::Kind::Book};
     for(const auto& k:books)if(k.symbol==gold.symbol){book=k;break;}
     preverified_root=inspect.summary().root_sha256;
     if(argc==8 && preverified_root!=std::string(argv[7]))
       throw std::runtime_error("recorded root differs from externally pinned expected root");
     std::cerr<<"verified_ma2_root="<<inspect.summary().root_sha256
              <<" dataset="<<inspect.metadata().dataset_id<<" origin="<<inspect.metadata().origin<<'\n';
   }else if(argc==8)throw std::invalid_argument("expected root pin is only available for MA2 traces");
 }catch(const std::exception& e){std::cerr<<"trace error: "<<e.what()<<'\n';return 1;}

 try{
   const std::string cohort=argc>=6?argv[5]:"expanded";
   if(cohort!="core" && cohort!="expanded" && cohort!="core-inc" && cohort!="expanded-inc")throw std::invalid_argument("invalid cohort");
   const bool with_legacy=cohort=="expanded"||cohort=="expanded-inc";
   const bool inc=cohort=="core-inc"||cohort=="expanded-inc";
   const auto make_graph=[&](){return ma::make_metal_graph(gold,silver,book,with_legacy,inc);};
   ma::Session session(make_graph());
   ma::PolicyOptions options;
   if(argc>=7 && std::string(argv[6])!="-"){
     if(std::string(argv[3])!="p")throw std::invalid_argument("frozen table is only used with P policy");
     session.set_frozen_costs(ma::load_frozen_cost_table(argv[6],make_graph()));
     options.cost_model=ma::CostModel::FrozenCalibration;
   }
   std::ifstream input;
   if(!is_ma2){input.open(argv[2]);if(!input)throw std::runtime_error("cannot read trace");}
   const auto policy=(argc>=4?parse_policy(argv[3]):ma::Policy::Cache);
   uint64_t budget=8'000'000;
   if(argc>=5){size_t end=0;const std::string v(argv[4]);budget=std::stoull(v,&end);
     if(end!=v.size()||budget==0)throw std::invalid_argument("budget must be positive");}
   int64_t previous_ingest_ns=0;
   const auto accept=[&](const ma::Event& e){
     if(e.ingest_ns<previous_ingest_ns)throw std::runtime_error("global ingestion times must be nondecreasing");
     previous_ingest_ns=e.ingest_ns;
     const auto status=session.store().ingest(e);
     if(status==ma::Ingest::Accepted){
       ++accepted;
       options.compute_budget_ns=budget;
       const auto result=session.execute(e.ingest_ns,policy,options);
       std::cout<<"event="<<accepted<<'|'<<ma::normalized_result(result);
       hits+=session.last_run().cache_hits;computes+=session.last_run().computations;
       deferred+=session.last_run().deferred;
     }else if(status==ma::Ingest::Duplicate)++dups;
     else throw std::runtime_error("invalid or out-of-order source event");
   };
   if(is_ma2){
     ma::TraceReader verified(argv[2]);ma::Event e;
     while(verified.next(e))accept(e);
     if(verified.summary().root_sha256!=preverified_root)
       throw std::runtime_error("recording changed between verification and replay");
     // A concurrent mutation of the on-disk trace between validation and replay
     // must never be ignored. The footer is independently checked in pass two.
   }else{
     std::string line;
     while(std::getline(input,line)){
       if(line.empty()||line[0]=='#')continue;
       accept(ma::decode_event(line));
     }
   }
 }catch(const std::exception& e){std::cerr<<"trace error: "<<e.what()<<'\n';return 1;}
 std::cerr<<"accepted="<<accepted<<" duplicates="<<dups<<" cache_hits="<<hits
          <<" computes="<<computes<<" deferred="<<deferred<<'\n';
 return 0;
}
