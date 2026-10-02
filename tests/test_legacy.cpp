#include "metalarch/legacy_extensions.hpp"
#include "metalarch/engines.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ma;
namespace {
int assertions=0;
#define CHECK(x) do{++assertions;if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+" failed: " #x);}while(0)
void close_to(double observed,double expected,double atol=1e-12){
 ++assertions;
 if(!std::isfinite(observed)||!std::isfinite(expected)||std::abs(observed-expected)>atol+1e-10*std::abs(expected))
   throw std::runtime_error("numerical oracle mismatch observed="+std::to_string(observed)+" expected="+std::to_string(expected));
}
const Key key{"fixture","XAU","1m",Kind::Bar};
constexpr int64_t T=2'000'000'000'000LL,STEP=60'000'000'000LL;
Event make_bar(uint64_t n,double close,double volume=100.0,double range=0.02){
 const auto ts=T+static_cast<int64_t>(n)*STEP;
 return {key,n,ts,ts+1'000'000'000LL,close,close*(1+range),close*(1-range),close,volume,Mode::Simulated};
}
void put(Session& s,uint64_t n,double close,double volume=100.0,double range=0.02){
 CHECK(s.store().ingest(make_bar(n,close,volume,range))==Ingest::Accepted);
}
Results run(Session& s,uint64_t n,Policy policy=Policy::SequentialFull){
 return s.execute(T+static_cast<int64_t>(n)*STEP+1'000'000'000LL,policy,{4,0});
}
double value(const Results& r,const char* id){
 const auto& item=r.at(id);CHECK(item.status==Status::Valid);CHECK(item.value.has_value());return *item.value;
}
void test_thresholds(){
 const Key peer{"fixture","XAG","1m",Kind::Bar},book{"fixture","XAU","live",Kind::Book};
 CHECK(make_metal_graph(key,peer,book,false).sorted().size()==11);
 CHECK(make_metal_graph(key,peer,book,true).sorted().size()==19);
 Session s(Graph(make_legacy_extensions(key)));
 CHECK(s.store().streams()==0);
 const auto empty=s.execute(T);
 CHECK(empty.size()==8);
 for(const auto& [id,r]:empty){CHECK(r.status==Status::Unavailable);CHECK(!r.value);}
 for(uint64_t i=1;i<=41;++i){
   put(s,i,100+static_cast<double>(i));
   const auto r=run(s,i);
   CHECK((r.at("roc12").status==Status::Valid)==(i>=13));
   CHECK((r.at("williams_r14").status==Status::Valid)==(i>=14));
   CHECK((r.at("cci20").status==Status::Valid)==(i>=20));
   CHECK((r.at("parkinson20").status==Status::Valid)==(i>=20));
   CHECK((r.at("garman_klass20").status==Status::Valid)==(i>=20));
   CHECK((r.at("amihud20").status==Status::Valid)==(i>=21));
   CHECK((r.at("volume_obv30").status==Status::Valid)==(i>=30));
   CHECK((r.at("cusum40").status==Status::Valid)==(i>=41));
   for(const auto& [id,x]:r){CHECK(x.mode==Mode::Simulated);if(x.status!=Status::Valid)CHECK(!x.value);}
 }
}
void test_independent_numerical_oracles(){
 Session s(Graph(make_legacy_extensions(key)));
 for(uint64_t i=1;i<=80;++i)put(s,i,100+static_cast<double>(i));
 const auto r=run(s,80);
 CHECK(r.size()==8);
 close_to(value(r,"roc12"),100.0*(180.0/168.0-1.0));
 close_to(value(r,"williams_r14"),-100.0*((180.0*1.02-180.0)/(180.0*1.02-167.0*.98)));
 // For an arithmetic progression of twenty typical prices [161,180], mean=170.5 and MAD=5.
 close_to(value(r,"cci20"),9.5/(0.015*5.0));
 const double log_hl=std::log(1.02/0.98);
 close_to(value(r,"parkinson20"),log_hl/std::sqrt(4.0*std::log(2.0)));
 close_to(value(r,"garman_klass20"),log_hl/std::sqrt(2.0));
 double oracle_amihud=0;
 for(int i=61;i<=80;++i){const double close=100.0+i;
   oracle_amihud+=std::abs(std::log(close/(close-1.0)))/(close*100.0);}
 close_to(value(r,"amihud20"),oracle_amihud/20.0,1e-18);
 // 30 rising bars, OBV advances by 100 per bar: slope / mean volume = 1.
 close_to(value(r,"volume_obv30"),0.5*std::tanh(1.0));
 const double alarm=value(r,"cusum40");
 CHECK(alarm>=0 && std::isfinite(alarm));
 for(const auto& [id,x]:r){CHECK(x.mode==Mode::Simulated);CHECK(!x.lineage.empty());CHECK(x.identity!=0);}
}
void test_degenerate_data(){
 Session flat(Graph(make_legacy_extensions(key)));
 for(uint64_t i=1;i<=80;++i)put(flat,i,100.0,100.0,0.0);
 const auto r=run(flat,80);
 CHECK(r.at("cci20").status==Status::Unavailable);
 CHECK(r.at("williams_r14").status==Status::Unavailable);
 close_to(value(r,"roc12"),0.0);
 close_to(value(r,"parkinson20"),0.0);
 close_to(value(r,"garman_klass20"),0.0);
 close_to(value(r,"amihud20"),0.0);
 close_to(value(r,"volume_obv30"),0.0);
 close_to(value(r,"cusum40"),0.0);
 Session zero_volume(Graph(make_legacy_extensions(key)));
 for(uint64_t i=1;i<=80;++i)put(zero_volume,i,100+static_cast<double>(i),i==80?0.0:100.0);
 const auto z=run(zero_volume,80);
 CHECK(z.at("amihud20").status==Status::Unavailable);
 CHECK(!z.at("amihud20").value);
 close_to(value(z,"volume_obv30"),0.0);
 CHECK(z.at("roc12").status==Status::Valid);
 // Zero volume must not be silently substituted by a price-derived proxy.
}
void test_differential_and_isolation(){
 Session full(Graph(make_legacy_extensions(key))),parallel(Graph(make_legacy_extensions(key))),cached(Graph(make_legacy_extensions(key)));
 for(uint64_t i=1;i<=90;++i){
   const double price=100.0+0.1*static_cast<double>(i)+2.0*std::sin(.7*static_cast<double>(i));
   for(Session* s:{&full,&parallel,&cached})put(*s,i,price);
   const auto a=run(full,i,Policy::SequentialFull);
   const auto b=run(parallel,i,Policy::ParallelFull);
   const auto c=run(cached,i,Policy::Cache);
   CHECK(normalized_result(a)==normalized_result(b));
   CHECK(normalized_result(a)==normalized_result(c));
   if(i==90){(void)run(cached,i,Policy::Cache);CHECK(cached.last_run().cache_hits==8);}
 }
 Session clean(Graph(make_legacy_extensions(key)));
 const auto r=clean.execute(T);
 for(const auto& [id,x]:r)CHECK(x.status==Status::Unavailable);
 CHECK(clean.hits()==0);
 // Touching one bar invalidates all stats declared against that stream.
 put(cached,91,150.0);
 const auto c=run(cached,91,Policy::Cache);
 CHECK(cached.last_run().cache_hits==0);
 CHECK(c.size()==8);
 // Wrong kind/source rejected by ReadView access controls.
 Descriptor bad=make_legacy_extensions(key).at(0);
 bad.id="undeclared_read";
 const Key extra{"fixture","OTHER","1m",Kind::Bar};
 bad.compute=[extra](const ReadView& v,const Results&){(void)v.get(extra);Result r;r.status=Status::Valid;r.value=1;return r;};
 Session restricted(Graph({bad}));put(restricted,1,101.0);
 CHECK(run(restricted,1).at("undeclared_read").status==Status::Failed);
}
}
int main(){
 try{test_thresholds();test_independent_numerical_oracles();test_degenerate_data();test_differential_and_isolation();
   std::cout<<"PASS "<<assertions<<" legacy-extension assertions across 4 groups\n";return 0;}
 catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
