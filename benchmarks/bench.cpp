#include "metalarch/engines.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>
using namespace ma;
int main(){
 const Key g{"fixture","XAU","1m",Kind::Bar}, p{"fixture","XAG","1m",Kind::Bar}, b{"fixture","XAU","live",Kind::Book};
 constexpr int64_t origin=1'000'000'000'000LL, step=60'000'000'000LL;
 auto graph=make_metal_graph(g,p,b);
 for(int strategy=0;strategy<2;++strategy){
  std::vector<double> durations;
  uint64_t all_hits=0,all_computes=0;double sink=0;
  for(int repeat=0;repeat<10;++repeat){
   Session s(graph);
   auto start=std::chrono::steady_clock::now();
   for(uint64_t i=1;i<=300;++i){
    int64_t ts=origin+static_cast<int64_t>(i)*step;
    auto make=[&](Key k,double close){return Event{k,i,ts,ts+3'000'000'000LL,close,close*1.01,close*0.99,close,100.0,Mode::Simulated};};
    double x=120.0+0.08*i+2.0*std::sin(i*0.6), y=90.0+0.12*i+1.5*std::cos(i*0.4);
    (void)s.store().ingest(make(g,x));(void)s.store().ingest(make(p,y));
    (void)s.store().ingest({b,i,ts,ts+3'000'000'000LL,100,101,100,50,0,Mode::Simulated});
    auto results=s.execute(ts+3'000'000'000LL,strategy==1);
    if(results.at("fusion").value)sink+=*results.at("fusion").value;
    // Identical second evaluation to exercise cache; reference recomputes it.
    auto again=s.execute(ts+3'000'000'000LL,strategy==1);
    if(again.at("fusion").value)sink+=*again.at("fusion").value;
   }
   auto end=std::chrono::steady_clock::now();
   durations.push_back(std::chrono::duration<double,std::milli>(end-start).count());
   all_hits+=s.hits();all_computes+=s.computations();
  }
  std::sort(durations.begin(),durations.end());
  std::cout<<(strategy==0?"B0 sequential-full":"B2 session-cache")
           <<" n=10 events=300 twice/eval synthetic mode=Simulated median_ms="
           <<std::fixed<<std::setprecision(3)<<(durations[4]+durations[5])/2.0
           <<" min_ms="<<durations.front()<<" max_ms="<<durations.back()
           <<" hits_total="<<all_hits<<" computes_total="<<all_computes<<" sink="<<sink<<'\n';
 }
}