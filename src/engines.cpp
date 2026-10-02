#include "metalarch/engines.hpp"
#include "metalarch/legacy_extensions.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iterator>
#include <numbers>
#include <stdexcept>
#include <vector>
namespace ma {
namespace {
Result ready(double v){Result r;r.status=Status::Valid;r.value=v;return r;}
Result missing(const char* why){Result r;r.reason=why;return r;}
const EventWindow& bars(const ReadView& s,const Key& k){auto p=s.get(k);if(!p)throw std::runtime_error("missing source");return p->events;}
double ema(const EventWindow& v, size_t n){
 if(v.empty())throw std::runtime_error("empty EMA input");
 const double alpha=2.0/(static_cast<double>(n)+1.0);
 double acc=v.front().d;for(size_t i=1;i<v.size();++i)acc=alpha*v[i].d+(1.0-alpha)*acc;return acc;
}
double val(const Results& r,const std::string& id){return *r.at(id).value;}
}
Graph make_metal_graph(const Key& primary,const Key& peer,const Key& book,bool include_legacy){
 constexpr int64_t BAR_TTL=120'000'000'000LL, BOOK_TTL=5'000'000'000LL;
 std::vector<Descriptor> d;
 d.push_back({"trend","ema-8-21-v1","fast=8;slow=21",{primary},{},BAR_TTL,[primary](const ReadView& s,const Results&){
   const auto& v=bars(s,primary);if(v.size()<21)return missing("21 bars required");
   return ready(100.0*(ema(v,8)-ema(v,21))/v.back().d);
 }});
 d.push_back({"momentum","rsi-wilder-v1","period=14",{primary},{},BAR_TTL,[primary](const ReadView& s,const Results&){
   const auto& v=bars(s,primary);if(v.size()<15)return missing("15 bars required");
   double gain=0,loss=0;
   for(size_t i=1;i<=14;++i){double change=v[i].d-v[i-1].d;gain+=std::max(change,0.0);loss+=std::max(-change,0.0);}
   gain/=14.0;loss/=14.0;
   for(size_t i=15;i<v.size();++i){double change=v[i].d-v[i-1].d;gain=(13.0*gain+std::max(change,0.0))/14.0;loss=(13.0*loss+std::max(-change,0.0))/14.0;}
   if(loss==0)return ready(gain==0?50.0:100.0);
   return ready(100.0-100.0/(1.0+gain/loss));
 }});
 d.push_back({"volatility","realized-rms-v1","window=20",{primary},{},BAR_TTL,[primary](const ReadView& s,const Results&){
   const auto& v=bars(s,primary);if(v.size()<21)return missing("21 bars required");
   double sum=0;for(size_t i=v.size()-20;i<v.size();++i){double ret=std::log(v[i].d/v[i-1].d);sum+=ret*ret;}
   return ready(std::sqrt(sum/20.0)); // per-bar RMS; no unverified annualization
 }});
 d.push_back({"atr","true-range-sma-v1","window=14",{primary},{},BAR_TTL,[primary](const ReadView& s,const Results&){
   const auto& v=bars(s,primary);if(v.size()<15)return missing("15 bars required");
   double sum=0;for(size_t i=v.size()-14;i<v.size();++i){
     sum+=std::max({v[i].b-v[i].c,std::abs(v[i].b-v[i-1].d),std::abs(v[i].c-v[i-1].d)});
   }return ready(sum/14.0);
 }});
 d.push_back({"peer_corr","pearson-matched-time-v1","returns=12",{primary,peer},{},BAR_TTL,[primary,peer](const ReadView& s,const Results&){
   const auto& a=bars(s,primary);const auto& b=bars(s,peer);
   if(a.size()<14||b.size()<14)return missing("14 observations per series required");
   std::vector<std::pair<double,double>> matched;
   size_t i=1,j=1;
   while(i<a.size()&&j<b.size()){
     if(a[i].event_ns==b[j].event_ns){
       if(a[i-1].event_ns==b[j-1].event_ns) matched.push_back({std::log(a[i].d/a[i-1].d),std::log(b[j].d/b[j-1].d)});
       ++i;++j;
     }
     else if(a[i].event_ns<b[j].event_ns)++i;else ++j;
   }
   if(matched.size()<12)return missing("12 matched return observations required");
   // Avoid unaligned returns: previous bar must also have identical event timestamp.
   double ax=0,by=0;for(auto [x,y]:matched){ax+=x;by+=y;}ax/=static_cast<double>(matched.size());by/=static_cast<double>(matched.size());
   double num=0,xx=0,yy=0;for(auto [x,y]:matched){x-=ax;y-=by;num+=x*y;xx+=x*x;yy+=y*y;}
   if(xx<=1e-28||yy<=1e-28)return missing("constant return series");
   return ready(std::clamp(num/std::sqrt(xx*yy),-1.0,1.0));
 }});
 d.push_back({"book_imbalance","top-of-book-imbalance-v1","snapshot=1",{book},{},BOOK_TTL,[book](const ReadView& s,const Results&){
   const auto& v=bars(s,book);const auto& e=v.back();double total=e.c+e.d;
   if(total<=0) return missing("empty book quantities");
   return ready((e.c-e.d)/total);
 }});
 d.push_back({"spectral","goertzel-concentration-v1","window=64;k=1..8",{primary},{},BAR_TTL,[primary](const ReadView& s,const Results&){
   const auto& v=bars(s,primary);if(v.size()<65)return missing("65 bars required");
   // Demeaned log returns and 8 Goertzel bins: deterministic bounded-cost spectrum proxy.
   double x[64];double mean=0;
   for(size_t j=0;j<64;++j){size_t i=v.size()-64+j;x[j]=std::log(v[i].d/v[i-1].d);mean+=x[j];}
   mean/=64.0;for(double& z:x)z-=mean;
   double total=0,top=0;
   for(int k=1;k<=8;++k){double coeff=2.0*std::cos(2.0*std::numbers::pi_v<double>*static_cast<double>(k)/64.0);double q0=0,q1=0,q2=0;
     for(double z:x){q0=z+coeff*q1-q2;q2=q1;q1=q0;}
     double power=std::max(0.0,q1*q1+q2*q2-coeff*q1*q2);total+=power;top=std::max(top,power);
   }
   return total<=1e-24?missing("flat returns"):ready(top/total);
 }});
 d.push_back({"regime","heuristic-regime-v1","scale=100",{}, {"trend","volatility"},BAR_TTL,[](const ReadView&,const Results& r){
   double v=val(r,"volatility"),t=val(r,"trend");return ready(std::tanh(t/(1e-8+100.0*v)));
 }});
 d.push_back({"risk","heuristic-risk-v1","scale=100",{}, {"volatility","atr"},BAR_TTL,[](const ReadView&,const Results& r){
   return ready(val(r,"volatility")+val(r,"atr")*0.001);
 }});
 d.push_back({"forecast","illustrative-forecast-v1","weights=0.7,0.3",{}, {"trend","momentum"},BAR_TTL,[](const ReadView&,const Results& r){
   // A deterministic illustration of dependency execution, NOT a trained/predictive forecast.
   return ready(0.7*val(r,"trend")+0.3*(val(r,"momentum")-50.0)/50.0);
 }});
 d.push_back({"fusion","illustrative-fusion-v1","weights=0.3,0.2,0.2,0.2,0.1",{}, {"regime","risk","forecast","peer_corr","book_imbalance"},BAR_TTL,[](const ReadView&,const Results& r){
   // Engineering workload only. No calibrated signal or investment advice implied.
   return ready(0.3*val(r,"regime")-0.2*val(r,"risk")+0.2*val(r,"forecast")+0.2*val(r,"peer_corr")+0.1*val(r,"book_imbalance"));
 }});
 // A deliberately slower spectral workload exercises both scheduling policies.
 for(auto& engine:d) {
   if(engine.id=="spectral") {engine.cadence=4;engine.estimated_cost_ns=4'000'000;}
   else if(engine.id=="peer_corr") engine.estimated_cost_ns=2'000'000;
   else if(engine.id=="fusion") engine.estimated_cost_ns=1'000'000;
 }
 if(include_legacy){
   auto extensions=make_legacy_extensions(primary);
   d.insert(d.end(),std::make_move_iterator(extensions.begin()),std::make_move_iterator(extensions.end()));
 }
 return Graph(std::move(d));
}
}
