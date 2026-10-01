#include "metalarch/engines.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace ma;
namespace {
int checks=0;
#define CHECK(x) do {++checks;if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " + #x);}while(0)
template<class F> void expect_throw(F&& f){bool thrown=false;try{f();}catch(const std::exception&){thrown=true;}CHECK(thrown);}
Key gold{"fixture","XAU","1m",Kind::Bar}, silver{"fixture","XAG","1m",Kind::Bar}, book{"fixture","XAU","live",Kind::Book};
constexpr int64_t T=1'000'000'000'000LL, STEP=60'000'000'000LL;
Event bar(Key k,uint64_t i,double close, Mode mode=Mode::Simulated){
 const int64_t ts=T+static_cast<int64_t>(i)*STEP;
 return {std::move(k),i,ts,ts+1'000'000'000LL,close,close*1.01,close*0.99,close,100.0+static_cast<double>(i),mode};
}
Event depth(uint64_t i,double buy=100,double sell=50){
 const int64_t ts=T+static_cast<int64_t>(i)*STEP;
 return {book,i,ts,ts+3'000'000'000LL,100,101,buy,sell,0,Mode::Simulated};
}
void feed(Session& s,uint64_t start,uint64_t end){
 for(uint64_t i=start;i<=end;++i){
   double x=120.0+0.08*static_cast<double>(i)+2.0*std::sin(static_cast<double>(i)*0.6);
   double y=90.0+0.12*static_cast<double>(i)+1.5*std::cos(static_cast<double>(i)*0.4);
   CHECK(s.store().ingest(bar(gold,i,x))==Ingest::Accepted);
   CHECK(s.store().ingest(bar(silver,i,y))==Ingest::Accepted);
   CHECK(s.store().ingest(depth(i))==Ingest::Accepted);
 }
}
void graph_tests(){
 Descriptor a{"a","1","",{}, {},0,[](const ReadView&,const Results&){Result r;r.status=Status::Valid;r.value=1;return r;}};
 Descriptor b=a;b.id="b";b.dependencies={"a"};
 Descriptor c=a;c.id="c";c.dependencies={"b"};
 Graph g({c,b,a});CHECK(g.sorted().size()==3);CHECK(g.sorted()[0].id=="a");CHECK(g.sorted()[2].id=="c");
 Descriptor invalid=b;invalid.dependencies={"unknown"};expect_throw([&]{Graph x({a,invalid});});
 Descriptor cyclic=a;cyclic.dependencies={"c"};expect_throw([&]{Graph x({cyclic,b,c});});
 expect_throw([&]{Graph x({a,a});});
 expect_throw([&]{Graph x({a,b,b});});
}
void ingestion_tests(){
 Store s;Event e=bar(gold,1,100);CHECK(s.ingest(e)==Ingest::Accepted);CHECK(s.version(gold)==1);
 CHECK(s.ingest(e)==Ingest::Duplicate);CHECK(s.version(gold)==1);
 auto altered=e;altered.b+=1;CHECK(s.ingest(altered)==Ingest::Invalid);CHECK(s.version(gold)==1);
 CHECK(s.ingest(bar(gold,0,99))==Ingest::Invalid);
 CHECK(s.ingest(bar(gold,2,-1))==Ingest::Invalid);
 CHECK(s.ingest(bar(gold,3,100))==Ingest::Accepted);CHECK(s.version(gold)==2);
 CHECK(s.ingest(bar(gold,2,100))==Ingest::OutOfOrder);
 auto bad=bar(gold,4,101);bad.event_ns=T;CHECK(s.ingest(bad)==Ingest::OutOfOrder);
 auto f=depth(1);CHECK(s.ingest(f)==Ingest::Accepted);
 CHECK(s.ingest(depth(2,0,0))==Ingest::Accepted); // zero quantities allowed but engine unavailable
 CHECK(s.streams()==2);
 Store alternative;CHECK(alternative.ingest(bar(gold,1,101))==Ingest::Accepted);
 CHECK(s.get(gold)->digest!=alternative.get(gold)->digest);
 Store capped(65);for(uint64_t i=1;i<=70;++i)CHECK(capped.ingest(bar(gold,i,100.0+static_cast<double>(i)))==Ingest::Accepted);
 CHECK(capped.version(gold)==70);CHECK(capped.get(gold)->events.size()==65);
 expect_throw([]{Store bad(20);});
}
void core_tests(){
 Session s(make_metal_graph(gold,silver,book));feed(s,1,70);
 int64_t now=T+70*STEP+3'000'000'000LL;
 auto ref=s.execute(now,false);CHECK(ref.size()==11);CHECK(ref.at("trend").mode==Mode::Simulated);
 for(const auto& [id,r]:ref){CHECK(r.status==Status::Valid);CHECK(r.value.has_value());CHECK(r.identity!=0);CHECK(!r.lineage.empty());}
 CHECK(ref.at("momentum").value.value()>=0&&ref.at("momentum").value.value()<=100);
 CHECK(ref.at("peer_corr").value.value()>=-1&&ref.at("peer_corr").value.value()<=1);
 CHECK(std::abs(*ref.at("book_imbalance").value-1.0/3.0)<1e-14);
 auto first=s.execute(now,true);CHECK(normalized_result(first)==normalized_result(ref));
 auto second=s.execute(now,true);CHECK(s.hits()>=11);CHECK(normalized_result(second)==normalized_result(ref));
 // Alter historical source while latest close is identical: new sequence should invalidate.
 auto old=s.store().version(gold);auto newest=bar(gold,71,122.0);newest.b=123.6;
 CHECK(s.store().ingest(newest)==Ingest::Accepted);CHECK(s.store().version(gold)==old+1);
 auto newer=s.execute(T+71*STEP+3'000'000'000LL,true);
 CHECK(newer.at("trend").identity!=first.at("trend").identity);
 CHECK(newer.at("peer_corr").identity!=first.at("peer_corr").identity);
 CHECK(newer.at("book_imbalance").status==Status::Stale);
 CHECK(newer.at("fusion").status==Status::Unavailable); // cannot pass stale book through fusion
 CHECK(!newer.at("fusion").value);
 // Resume peer/book, then cached and uncached must agree.
 CHECK(s.store().ingest(bar(silver,71,93.0))==Ingest::Accepted);
 CHECK(s.store().ingest(depth(71))==Ingest::Accepted);
 const auto uncached=s.execute(T+71*STEP+3'000'000'000LL,false);
 const auto cached=s.execute(T+71*STEP+3'000'000'000LL,true);
 CHECK(normalized_result(uncached)==normalized_result(cached));
 // Source becomes stale even when signature is unchanged (TTL checked before cache hits).
 const auto stale=s.execute(T+71*STEP+200'000'000'000LL,true);
 CHECK(stale.at("trend").status==Status::Stale);CHECK(stale.at("book_imbalance").status==Status::Stale);
 CHECK(stale.at("fusion").status==Status::Unavailable);CHECK(!stale.at("fusion").value);
 // Per-session and reset isolation.
 Session other(make_metal_graph(gold,silver,book));auto empty=other.execute(now);
 CHECK(empty.at("trend").status==Status::Unavailable);CHECK(other.store().streams()==0);
 s.reset();CHECK(s.store().streams()==0);CHECK(s.hits()==0);CHECK(s.computations()==0);
}
void failure_tests(){
 int invoked=0;
 Descriptor x{"x","1","",{gold},{},0,[&](const ReadView&,const Results&)->Result{++invoked;throw std::runtime_error("injected");}};
 Descriptor y{"y","1","",{}, {"x"},0,[](const ReadView&,const Results&){Result r;r.status=Status::Valid;r.value=50;return r;}};
 Session s(Graph({x,y}));CHECK(s.store().ingest(bar(gold,1,100))==Ingest::Accepted);
 auto r=s.execute(T+STEP+1'000'000'000LL);
 CHECK(r.at("x").status==Status::Failed);CHECK(r.at("y").status==Status::Unavailable);CHECK(!r.at("x").value);
 (void)s.execute(T+STEP+1'000'000'000LL);CHECK(invoked==2); // failure not cached
 auto good=[](const ReadView&,const Results&){Result r;r.status=Status::Valid;r.value=50;return r;};
 Descriptor neutral{"neutral","1","",{gold},{},0,good};Session n(Graph({neutral}));
 CHECK(n.store().ingest(bar(gold,1,100))==Ingest::Accepted);
 auto v=n.execute(T+STEP+1'000'000'000LL);CHECK(v.at("neutral").status==Status::Valid);CHECK(*v.at("neutral").value==50);
 Descriptor illegal{"illegal","1","",{gold},{},0,[](const ReadView& view,const Results&){
   (void)view.get(silver);Result r;r.status=Status::Valid;r.value=1;return r;
 }};
 Session restricted(Graph({illegal}));CHECK(restricted.store().ingest(bar(gold,1,100))==Ingest::Accepted);
 CHECK(restricted.execute(T+STEP+1'000'000'000LL).at("illegal").status==Status::Failed);
}
void replay_tests(){
 Session one(make_metal_graph(gold,silver,book)),two(make_metal_graph(gold,silver,book));
 std::vector<std::string> trace;
 for(uint64_t i=1;i<=70;++i){
   double p=120.0+0.08*static_cast<double>(i)+2.0*std::sin(static_cast<double>(i)*0.6);
   double q=90.0+0.12*static_cast<double>(i)+1.5*std::cos(static_cast<double>(i)*0.4);
   trace.push_back(encode_event(bar(gold,i,p)));trace.push_back(encode_event(bar(silver,i,q)));
   trace.push_back(encode_event(depth(i)));
 }
 for(const auto& line:trace){
   Event e=decode_event(line);CHECK(encode_event(e)==line);
   CHECK(one.store().ingest(e)==Ingest::Accepted);
   CHECK(two.store().ingest(decode_event(line))==Ingest::Accepted);
   auto a=one.execute(e.ingest_ns),b=two.execute(e.ingest_ns);
   CHECK(normalized_result(a)==normalized_result(b));
 }
 CHECK(one.hits()==two.hits());
 expect_throw([]{decode_event("MA1|B|malformed");});
 auto bad=trace[0];bad+="|1";expect_throw([&]{decode_event(bad);});
 CHECK(one.store().ingest(decode_event(trace.back()))==Ingest::Duplicate);
}
}
int main(){
 try{graph_tests();ingestion_tests();core_tests();failure_tests();replay_tests();std::cout<<"PASS "<<checks<<" assertions across 5 test groups\n";return 0;}
 catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}