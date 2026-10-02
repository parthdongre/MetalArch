#include "metalarch/engines.hpp"
#include <cmath>
#include <atomic>
#include <chrono>
#include <thread>
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
 Descriptor source_dup=a;source_dup.id="dup_src";source_dup.sources={gold,gold};
 expect_throw([&]{Graph x({source_dup});});
}
void ingestion_tests(){
 Store s;Event e=bar(gold,1,100);CHECK(s.ingest(e)==Ingest::Accepted);CHECK(s.version(gold)==1);
 CHECK(s.ingest(e)==Ingest::Duplicate);CHECK(s.version(gold)==1);
 auto invalid_mode=e;invalid_mode.seq=2;invalid_mode.mode=static_cast<Mode>(99);
 CHECK(s.ingest(invalid_mode)==Ingest::Invalid);
 auto invalid_kind=e;invalid_kind.key.kind=static_cast<Kind>(99);
 CHECK(s.ingest(invalid_kind)==Ingest::Invalid);
 auto altered=e;altered.b+=1;CHECK(s.ingest(altered)==Ingest::Invalid);CHECK(s.version(gold)==1);
 CHECK(s.ingest(bar(gold,0,99))==Ingest::Invalid);
 CHECK(s.ingest(bar(gold,2,-1))==Ingest::Invalid);
 CHECK(s.ingest(bar(gold,3,100))==Ingest::Accepted);CHECK(s.version(gold)==2);
 CHECK(s.ingest(bar(gold,2,100))==Ingest::OutOfOrder);
 Descriptor raw{"raw","1","",{gold},{},0,[](const ReadView& view,const Results&){
   Result r;r.status=Status::Valid;r.value=view.get(gold)->events.back().d;return r;
 }};
 Session causal(Graph({raw}));auto future=bar(gold,1,100);future.ingest_ns+=10'000'000'000LL;
 CHECK(causal.store().ingest(future)==Ingest::Accepted);
 CHECK(causal.execute(future.ingest_ns-1).at("raw").status==Status::Unavailable);
 CHECK(causal.execute(future.ingest_ns).at("raw").status==Status::Valid);
 auto bad=bar(gold,4,101);bad.event_ns=T;CHECK(s.ingest(bad)==Ingest::OutOfOrder);
 auto f=depth(1);CHECK(s.ingest(f)==Ingest::Accepted);
 CHECK(s.ingest(depth(2,0,0))==Ingest::Accepted); // zero quantities allowed but engine unavailable
 CHECK(s.streams()==2);
 Store alternative;CHECK(alternative.ingest(bar(gold,1,101))==Ingest::Accepted);
 CHECK(s.get(gold)->digest!=alternative.get(gold)->digest);
 Store capped(65);for(uint64_t i=1;i<=70;++i)CHECK(capped.ingest(bar(gold,i,100.0+static_cast<double>(i)))==Ingest::Accepted);
 CHECK(capped.version(gold)==70);CHECK(capped.get(gold)->events.size()==65);
 CHECK(capped.get(gold)->events.front().seq==6);
 CHECK(capped.get(gold)->events.back().seq==70);
 for(size_t i=0;i<65;++i)CHECK(capped.get(gold)->events[i].seq==static_cast<uint64_t>(i+6));
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
 CHECK(newer.at("fusion").mode==Mode::Simulated);
 CHECK(newer.at("fusion").upstream_versions.size()==5);
 // Resume peer/book, then cached and uncached must agree.
 CHECK(s.store().ingest(bar(silver,71,93.0))==Ingest::Accepted);
 CHECK(s.store().ingest(depth(71))==Ingest::Accepted);
 const auto uncached=s.execute(T+71*STEP+3'000'000'000LL,false);
 const auto cached=s.execute(T+71*STEP+3'000'000'000LL,true);
 CHECK(normalized_result(uncached)==normalized_result(cached));
 // A book-only update must not invalidate bar-only computations.
 auto changed_book=depth(72,200,50);
 changed_book.event_ns=T+71*STEP;changed_book.ingest_ns=T+71*STEP+4'000'000'000LL;
 CHECK(s.store().ingest(changed_book)==Ingest::Accepted);
 const auto changed=s.execute(changed_book.ingest_ns,true);
 CHECK(changed.at("trend").identity==cached.at("trend").identity);
 CHECK(changed.at("book_imbalance").identity!=cached.at("book_imbalance").identity);
 CHECK(changed.at("fusion").identity!=cached.at("fusion").identity);
 CHECK(std::abs(*changed.at("book_imbalance").value-0.6)<1e-12);
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
void policy_tests(){
  // Differential tests: same input cuts must produce identical normalized
  // mathematical outputs in uncached sequential, uncached DAG-parallel, cache.
  Session b0(make_metal_graph(gold,silver,book));
  Session b1(make_metal_graph(gold,silver,book));
  Session b2(make_metal_graph(gold,silver,book));
  for(uint64_t i=1;i<=90;++i){
    feed(b0,i,i);feed(b1,i,i);feed(b2,i,i);
    const int64_t t=T+static_cast<int64_t>(i)*STEP+3'000'000'000LL;
    auto reference=b0.execute(t,Policy::SequentialFull);
    auto parallel=b1.execute(t,Policy::ParallelFull,{4,0});
    auto cached=b2.execute(t,Policy::Cache);
    CHECK(normalized_result(reference)==normalized_result(parallel));
    CHECK(normalized_result(reference)==normalized_result(cached));
    CHECK(b1.last_run().deferred==0);
    CHECK(b1.last_run().execution_order.size()==11);
    CHECK(b0.last_run().cache_hits==0);
    const auto again=b2.execute(t,Policy::Cache);
    CHECK(normalized_result(reference)==normalized_result(again));
  }
  // B1 actually overlaps independent nodes; a dependent node must wait.
  std::atomic<int> running{0}, peak{0};
  auto work=[&](const ReadView&,const Results&){
    int cur=++running;int old=peak.load();
    while(cur>old&&!peak.compare_exchange_weak(old,cur)){}
    std::this_thread::sleep_for(std::chrono::milliseconds(9));
    --running;Result r;r.status=Status::Valid;r.value=1;return r;
  };
  Descriptor a{"a","1","",{}, {},0,work};
  Descriptor b=a;b.id="b";
  Descriptor c{"c","1","",{}, {"a","b"},0,[](const ReadView&,const Results& upstream){
    Result r;r.status=Status::Valid;r.value=*upstream.at("a").value+*upstream.at("b").value;return r;
  }};
  Session concurrent(Graph({c,b,a}));
  auto result=concurrent.execute(T,Policy::ParallelFull,{2,0});
  CHECK(peak.load()>=2);CHECK(*result.at("c").value==2);
  CHECK(concurrent.last_run().computations==3);
  expect_throw([&]{(void)concurrent.execute(T-1,Policy::ParallelFull);});
  expect_throw([&]{(void)concurrent.execute(T,Policy::ParallelFull,{0,0});});
  // Independent computed-result TTL: identical source digest alone does not
  // authorize indefinite reuse.
  Descriptor expiring{"ttl","1","",{gold},{},120'000'000'000LL,
    [=](const ReadView& rv,const Results&){Result r;r.status=Status::Valid;r.value=rv.get(gold)->events.back().d;return r;}};
  expiring.max_result_age_ns=2'000'000'000LL;
  Session age(Graph({expiring}));CHECK(age.store().ingest(bar(gold,1,100))==Ingest::Accepted);
  auto t=T+STEP+1'000'000'000LL;
  (void)age.execute(t,Policy::Cache);CHECK(age.last_run().computations==1);
  (void)age.execute(t+1'000'000'000LL,Policy::Cache);CHECK(age.last_run().cache_hits==1);
  (void)age.execute(t+3'000'000'000LL,Policy::Cache);CHECK(age.last_run().computations==1);
  CHECK(age.computations()==2);
  // Descriptor contracts reject impossible policy specifications.
  Descriptor invalid=expiring;invalid.cadence=0;expect_throw([&]{Graph g({invalid});});
  invalid=expiring;invalid.estimated_cost_ns=0;expect_throw([&]{Graph g({invalid});});
  invalid=expiring;invalid.max_result_age_ns=-1;expect_throw([&]{Graph g({invalid});});
}
void scheduling_tests(){
  const int64_t now=T+70*STEP+3'000'000'000LL;
  Descriptor slow{"slow","1","",{gold},{},120'000'000'000LL,
    [&](const ReadView& v,const Results&){Result r;r.status=Status::Valid;r.value=v.get(gold)->events.back().d;return r;}};
  slow.cadence=3;slow.estimated_cost_ns=2;
  Descriptor child{"child","1","",{}, {"slow"},120'000'000'000LL,
    [](const ReadView&,const Results& p){Result r;r.status=Status::Valid;r.value=*p.at("slow").value+1;return r;}};
  Session cadence(Graph({child,slow}));
  CHECK(cadence.store().ingest(bar(gold,70,100))==Ingest::Accepted);
  const auto original=cadence.execute(now,Policy::FixedCadence);
  CHECK(original.at("slow").status==Status::Valid);
  CHECK(cadence.store().ingest(bar(gold,71,110))==Ingest::Accepted);
  auto t=T+71*STEP+1'000'000'000LL;
  const auto deferred=cadence.execute(t,Policy::FixedCadence);
  CHECK(deferred.at("slow").status==Status::Stale);
  CHECK(!deferred.at("slow").value);
  CHECK(deferred.at("slow").last_known_value==original.at("slow").value);
  CHECK(deferred.at("slow").last_known_identity==original.at("slow").identity);
  CHECK(deferred.at("slow").last_known_source_ns==original.at("slow").source_ns);
  CHECK(deferred.at("slow").last_known_computed_ns==original.at("slow").computed_ns);
  CHECK(deferred.at("slow").identity!=original.at("slow").identity);
  CHECK(deferred.at("child").status==Status::Unavailable);
  CHECK(cadence.last_run().deferred==1);
  (void)cadence.execute(t,Policy::FixedCadence);
  auto resumed=cadence.execute(t,Policy::FixedCadence);
  CHECK(resumed.at("slow").status==Status::Valid);
  CHECK(resumed.at("child").status==Status::Valid);
  // Deadline queue prioritizes the dependency closure of an urgent node.
  Descriptor root{"root","1","",{gold},{},120'000'000'000LL,
    [](const ReadView& v,const Results&){Result r;r.status=Status::Valid;r.value=v.get(gold)->events.back().d;return r;}};
  root.estimated_cost_ns=2;
  Descriptor urgent{"urgent","1","",{book},{"root"},5'000'000'000LL,
    [](const ReadView& v,const Results& p){Result r;r.status=Status::Valid;r.value=*p.at("root").value+v.get(book)->events.back().c;return r;}};
  urgent.estimated_cost_ns=2;
  Descriptor leisurely{"leisurely","1","",{silver},{},120'000'000'000LL,
    [](const ReadView& v,const Results&){Result r;r.status=Status::Valid;r.value=v.get(silver)->events.back().d;return r;}};
  leisurely.estimated_cost_ns=2;
  Session constrained(Graph({leisurely,urgent,root}));
  CHECK(constrained.store().ingest(bar(gold,70,100))==Ingest::Accepted);
  CHECK(constrained.store().ingest(bar(silver,70,90))==Ingest::Accepted);
  CHECK(constrained.store().ingest(depth(70))==Ingest::Accepted);
  auto scheduled=constrained.execute(now,Policy::Freshness,{1,4});
  CHECK(constrained.last_run().computations==2);
  CHECK(constrained.last_run().estimated_cost_ns==4);
  CHECK(constrained.last_run().execution_order.front()=="root");
  CHECK(scheduled.at("root").status==Status::Valid);
  CHECK(scheduled.at("urgent").status==Status::Valid);
  CHECK(scheduled.at("leisurely").status==Status::Unavailable);
  CHECK(constrained.last_run().deferred==1);
  // Cached work consumes no new budget, leaving room for the delayed node.
  scheduled=constrained.execute(now,Policy::Freshness,{1,4});
  CHECK(scheduled.at("urgent").status==Status::Valid);
  CHECK(scheduled.at("leisurely").status==Status::Valid);
  CHECK(constrained.last_run().cache_hits==2);
  CHECK(constrained.last_run().estimated_cost_ns==2);
  // No budget cap must agree with cached execution on the same input cut.
  Session free(Graph({leisurely,urgent,root}));
  CHECK(free.store().ingest(bar(gold,70,100))==Ingest::Accepted);
  CHECK(free.store().ingest(bar(silver,70,90))==Ingest::Accepted);
  CHECK(free.store().ingest(depth(70))==Ingest::Accepted);
  const auto unlimited=free.execute(now,Policy::Freshness,{1,0});
  Session reference(Graph({leisurely,urgent,root}));
  CHECK(reference.store().ingest(bar(gold,70,100))==Ingest::Accepted);
  CHECK(reference.store().ingest(bar(silver,70,90))==Ingest::Accepted);
  CHECK(reference.store().ingest(depth(70))==Ingest::Accepted);
  CHECK(normalized_result(unlimited)==normalized_result(reference.execute(now,Policy::SequentialFull)));
  CHECK(free.last_run().deferred==0);
  // On a changed input with no compute budget, old values are only stale
  // metadata and must not be consumed by the dependent node.
  CHECK(constrained.store().ingest(bar(gold,71,101))==Ingest::Accepted);
  CHECK(constrained.store().ingest(bar(silver,71,91))==Ingest::Accepted);
  CHECK(constrained.store().ingest(depth(71))==Ingest::Accepted);
  // Budget 1 cannot afford the changed root (cost 2).
  auto later=constrained.execute(T+71*STEP+3'000'000'000LL,Policy::Freshness,{1,1});
  CHECK(later.at("root").status==Status::Stale);
  CHECK(!later.at("root").value);
  CHECK(later.at("root").last_known_value.has_value());
  CHECK(later.at("urgent").status==Status::Unavailable);
  CHECK(constrained.last_run().estimated_cost_ns<=1);
}
void policy_replay_tests(){
  Session fixed_a(make_metal_graph(gold,silver,book)),fixed_b(make_metal_graph(gold,silver,book));
  Session budget_a(make_metal_graph(gold,silver,book)),budget_b(make_metal_graph(gold,silver,book));
  for(uint64_t i=1;i<=82;++i){
    const double x=120.0+0.08*static_cast<double>(i)+2.0*std::sin(static_cast<double>(i)*0.6);
    const double y=90.0+0.12*static_cast<double>(i)+1.5*std::cos(static_cast<double>(i)*0.4);
    for(const auto& e:{bar(gold,i,x),bar(silver,i,y),depth(i)}){
      const auto encoded=encode_event(e);
      for(Session* ss:{&fixed_a,&fixed_b,&budget_a,&budget_b}){
        CHECK(ss->store().ingest(decode_event(encoded))==Ingest::Accepted);
      }
      auto f1=fixed_a.execute(e.ingest_ns,Policy::FixedCadence);
      auto f2=fixed_b.execute(e.ingest_ns,Policy::FixedCadence);
      CHECK(normalized_result(f1)==normalized_result(f2));
      auto p1=budget_a.execute(e.ingest_ns,Policy::Freshness,{3,8'000'000});
      auto p2=budget_b.execute(e.ingest_ns,Policy::Freshness,{3,8'000'000});
      CHECK(normalized_result(p1)==normalized_result(p2));
      CHECK(budget_a.last_run().estimated_cost_ns<=8'000'000);
      CHECK(budget_b.last_run().estimated_cost_ns<=8'000'000);
    }
  }
  // Exceptions in a concurrent engine do not escape workers or make a valid
  // neutral placeholder available to downstream consumers.
  Descriptor exploding{"explode","1","",{}, {},0,[](const ReadView&,const Results&)->Result{
    throw std::runtime_error("intentional parallel failure");
  }};
  Descriptor healthy{"healthy","1","",{}, {},0,[](const ReadView&,const Results&)->Result{
    Result r;r.status=Status::Valid;r.value=5;return r;
  }};
  Descriptor dependent{"dependent","1","",{}, {"explode"},0,[](const ReadView&,const Results&)->Result{
    throw std::runtime_error("blocked child must never execute");
  }};
  Session s(Graph({dependent,healthy,exploding}));
  auto r=s.execute(T,Policy::ParallelFull,{2,0});
  CHECK(r.at("explode").status==Status::Failed);
  CHECK(r.at("healthy").status==Status::Valid);
  CHECK(r.at("dependent").status==Status::Unavailable);
  CHECK(s.last_run().computations==2);
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
 try{graph_tests();ingestion_tests();core_tests();failure_tests();replay_tests();policy_tests();scheduling_tests();policy_replay_tests();std::cout<<"PASS "<<checks<<" assertions across 8 test groups\n";return 0;}
 catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
