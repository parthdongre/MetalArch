#include "metalarch/core.hpp"
#include "metalarch/engines.hpp"
#include "metalarch/legacy_extensions.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ma;
namespace {
int checks=0;
double largest_diff=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" failed " #x);}while(0)
const Key primary{"fixture","XAU","1m",Kind::Bar},peer{"fixture","XAG","1m",Kind::Bar};
const Key book{"fixture","XAU","live",Kind::Book};
constexpr int64_t EPOCH=5'000'000'000'000'000LL, STEP=60'000'000'000LL;
Event bar(const Key& key,uint64_t n,double price,double volume=100.0,bool flat=false){
 const int64_t t=EPOCH+static_cast<int64_t>(n)*STEP;
 return {key,n,t,t+1'000'000'000LL,price,flat?price:price*1.014,flat?price:price*.986,price,volume,Mode::Simulated};
}
Event depth(uint64_t n){
 const auto t=EPOCH+static_cast<int64_t>(n)*STEP;
 return {book,n,t,t+1'000'000'000LL,100,101,25+double(n%30),32,0,Mode::Simulated};
}
void compare(const Results& reference,const Results& incremental){
 CHECK(reference.size()==incremental.size());
 for(const auto& [id,a]:reference){
  const auto& b=incremental.at(id);
  CHECK(a.status==b.status);CHECK(a.mode==b.mode);CHECK(a.source_ns==b.source_ns);
  CHECK(a.reason==b.reason);CHECK(a.value.has_value()==b.value.has_value());
  if(a.value){
   const double diff=std::abs(*a.value-*b.value);
   CHECK(std::isfinite(*a.value)&&std::isfinite(*b.value));
   largest_diff=std::max(largest_diff,diff);
   CHECK(diff<=2e-11+1e-9*std::abs(*a.value));
  }
  // Version-revision IDs are intentionally different for the accelerated
  // engines and their descendants; raw fingerprint equality is inappropriate.
 }
}
void test_descriptor_isolation(){
 const auto baseline=make_metal_graph(primary,peer,book,true,false);
 const auto faster=make_metal_graph(primary,peer,book,true,true);
 CHECK(baseline.sorted().size()==19);CHECK(faster.sorted().size()==19);
 int upgraded=0;
 for(const auto& d:faster.sorted())if(d.requires_recent20){
  ++upgraded;CHECK(d.sources.size()==1);CHECK(d.sources[0]==primary);
  const auto& ref=*std::find_if(baseline.sorted().begin(),baseline.sorted().end(),[&](const Descriptor& x){return x.id==d.id;});
  CHECK(ref.revision!=d.revision);CHECK(!ref.requires_recent20);
 }
 CHECK(upgraded==4);
 Session normal(make_metal_graph(primary,peer,book,true,false));
 Session fast(make_metal_graph(primary,peer,book,true,true));
 CHECK(normal.store().get(primary)==nullptr);CHECK(fast.store().get(primary)==nullptr);
 const auto e=bar(primary,1,101);
 CHECK(normal.store().ingest(e)==Ingest::Accepted);CHECK(fast.store().ingest(e)==Ingest::Accepted);
 CHECK(normal.store().get(primary)->recent20==nullptr);
 CHECK(fast.store().get(primary)->recent20!=nullptr);
 CHECK(!fast.store().get(primary)->recent20->full());
 const auto version=fast.store().version(primary);
 const auto before=fast.store().get(primary)->recent20->park_sum;
 CHECK(fast.store().ingest(e)==Ingest::Duplicate);
 CHECK(fast.store().version(primary)==version);
 CHECK(fast.store().get(primary)->recent20->park_sum==before);
 auto wrong=e;wrong.seq=0;
 CHECK(fast.store().ingest(wrong)==Ingest::Invalid);
 CHECK(fast.store().version(primary)==version);
 CHECK(fast.store().get(primary)->recent20->park_sum==before);
 CHECK(fast.store().ingest(depth(1))==Ingest::Accepted);
 CHECK(fast.store().get(book)->recent20==nullptr);
 fast.reset();CHECK(fast.store().streams()==0);
 CHECK(fast.store().ingest(e)==Ingest::Accepted);
 CHECK(fast.store().get(primary)->recent20!=nullptr);
}
void test_wraparound_and_numerics(){
 Session baseline(make_metal_graph(primary,peer,book,true,false));
 Session fast(make_metal_graph(primary,peer,book,true,true));
 const std::vector<uint64_t> checkpoints{1,13,14,19,20,21,30,40,41,64,65,70,80,99,100,120,1000,4000,4095,4096,4097,4100,4120,4500,5000,5100,5120,5200};
 for(uint64_t i=1;i<=5200;++i){
  const double price=120.0+.03*double(i)+1.2*std::sin(.13*double(i));
  const double ppeer=100.0+.011*double(i)+.5*std::cos(.09*double(i));
  const bool flat=i%73==0;
  const double volume=(i==21||i==80||i==4095||i==4105||i==5100)?0.0:100.0+double(i%33);
  auto e=bar(primary,i,price,volume,flat),p=bar(peer,i,ppeer),bk=depth(i);
  for(Session* s:{&baseline,&fast}){
   CHECK(s->store().ingest(e)==Ingest::Accepted);
   CHECK(s->store().ingest(p)==Ingest::Accepted);
   CHECK(s->store().ingest(bk)==Ingest::Accepted);
  }
  if(std::binary_search(checkpoints.begin(),checkpoints.end(),i)){
   const auto timestamp=e.ingest_ns;
   compare(baseline.execute(timestamp,Policy::SequentialFull),fast.execute(timestamp,Policy::SequentialFull));
   const auto* r=fast.store().get(primary)->recent20.get();CHECK(r!=nullptr);
   if(i>=20){CHECK(r->full());CHECK(r->count==20);CHECK(r->park_good==20);CHECK(r->gk_good==20);}
   if(i>=21){CHECK(r->return_good==20);}
   if(i==4097)CHECK(fast.store().get(primary)->events.size()==4096);
  }
 }
 CHECK(largest_diff<2e-11);
}
void test_all_policies_and_replay(const std::string& path){
 std::ifstream in(path);if(!in)throw std::runtime_error("fixture not accessible");
 std::vector<Event> events;std::string line;
 while(std::getline(in,line))if(!line.empty()&&line[0]!='#')events.push_back(decode_event(line));
 CHECK(events.size()==210);
 for(bool expanded:{false,true})for(Policy policy:{Policy::SequentialFull,Policy::ParallelFull,Policy::Cache,Policy::FixedCadence,Policy::Freshness}){
  Session a(make_metal_graph(primary,peer,book,expanded,false));
  Session b(make_metal_graph(primary,peer,book,expanded,true));
  PolicyOptions opts;opts.compute_budget_ns=8'000'000;
  for(const auto& e:events){
   CHECK(a.store().ingest(e)==Ingest::Accepted);CHECK(b.store().ingest(e)==Ingest::Accepted);
   const auto ra=a.execute(e.ingest_ns,policy,opts),rb=b.execute(e.ingest_ns,policy,opts);
   compare(ra,rb);
   CHECK(a.last_run().computations==b.last_run().computations);
   CHECK(a.last_run().deferred==b.last_run().deferred);
  }
 }
}
void test_late_activation(){
 Store store(65);
 for(uint64_t i=1;i<=200;++i)CHECK(store.ingest(bar(primary,i,100.0+double(i)*.13))==Ingest::Accepted);
 CHECK(store.get(primary)->recent20==nullptr);
 store.enable_recent20(primary);
 const auto* s=store.get(primary);
 CHECK(s->recent20!=nullptr);CHECK(s->recent20->full());CHECK(s->recent20->return_good==20);
 double oracle=0;
 for(size_t i=s->events.size()-20;i<s->events.size();++i){const auto x=std::log(s->events[i].d/s->events[i-1].d);oracle+=x*x;}
 CHECK(std::abs(s->recent20->return_sq_sum-oracle)<1e-14);
 // Wrapping at the minimum legal Store capacity cannot break the 20-bar terms.
 for(uint64_t i=201;i<=290;++i)CHECK(store.ingest(bar(primary,i,100.0+double(i)*.13))==Ingest::Accepted);
 s=store.get(primary);CHECK(s->events.size()==65);CHECK(s->recent20->full());
 oracle=0;
 for(size_t i=s->events.size()-20;i<s->events.size();++i){const auto x=std::log(s->events[i].d/s->events[i-1].d);oracle+=x*x;}
 CHECK(std::abs(s->recent20->return_sq_sum-oracle)<1e-14);
}
}
int main(int argc,char** argv){
 try{
  if(argc!=2)throw std::invalid_argument("expected MA1 fixture path");
  test_descriptor_isolation();test_wraparound_and_numerics();test_late_activation();test_all_policies_and_replay(argv[1]);
  std::cout<<"PASS "<<checks<<" incremental assertions, 4 groups; max absolute feature deviation="<<largest_diff
           <<"; optional feature-index bytes per enabled stream="<<sizeof(Recent20)<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
