#include "metalarch/core.hpp"
#include "metalarch/resource.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ma;
namespace {
int checks=0;
#define CHECK(condition) do {++checks;if(!(condition))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " + #condition);}while(0)
template<class F> void throws(F f){bool yes=false;try{f();}catch(const std::exception&){yes=true;}CHECK(yes);}
const Key gold{"fixture","XAU","1m",Kind::Bar};
Event event(){return {gold,1,1'000'000,1'000'001,100,102,99,101,100,Mode::Simulated};}
Descriptor descriptor(std::string id){
  Descriptor d{std::move(id),"rev1","period=1",{gold},{},100'000,
    [](const ReadView& v,const Results&){Result out;out.status=Status::Valid;out.value=v.get(gold)->events.back().d;return out;}};
  d.estimated_cost_ns=500'000;
  return d;
}
std::string table_header(){return "id\trevision\tparameters\tdescriptor_digest\tvalid_samples\twall_p50_ns\twall_p95_ns\tthread_cpu_p95_ns\tcost_ns\n";}
std::string table_row(std::string id,std::string rev="rev1",std::string samples="5",std::string cost="1000"){
 return id+"\t"+rev+"\tperiod=1\t"+std::to_string(descriptor_signature(descriptor(id)))+
   "\t"+samples+"\t200\t400\t350\t"+cost+"\n";
}
void test_sampler(){
 const auto before=sample_process_resources(),after=sample_process_resources();
 CHECK(after.process_cpu_ns>=before.process_cpu_ns);
 if(after.current_rss_supported)CHECK(after.current_rss_bytes>0);
 if(after.peak_rss_supported)CHECK(after.peak_rss_bytes>0);
 const auto thread_a=sample_thread_cpu_ns(),thread_b=sample_thread_cpu_ns();
 if(thread_a && thread_b)CHECK(*thread_b>=*thread_a);
}
void test_lazy_window_storage(){
  Store s;CHECK(s.reserved_event_payload_bytes_lower_bound()==0);
  CHECK(s.ingest(event())==Ingest::Accepted);
  CHECK(s.get(gold)->events.reserved_slots()<=64);
  CHECK(s.reserved_event_payload_bytes_lower_bound()==s.get(gold)->events.reserved_slots()*sizeof(Event));
  EventWindow small(65);CHECK(small.reserved_slots()<=65);
  for(uint64_t i=1;i<=70;++i){auto e=event();e.seq=i;small.push(e);}
  CHECK(small.size()==65);CHECK(small.front().seq==6);CHECK(small.back().seq==70);
  CHECK(small.reserved_slots()==65);
  throws([]{EventWindow invalid(0);});
}
void test_opt_in(){
 Graph graph({descriptor("a"),descriptor("b")});
 Session plain(graph),profiled(graph);
 CHECK(plain.store().ingest(event())==Ingest::Accepted);
 CHECK(profiled.store().ingest(event())==Ingest::Accepted);
 PolicyOptions opts;opts.profile_resources=true;
 const auto r0=plain.execute(1'000'001,Policy::SequentialFull);
 const auto r1=profiled.execute(1'000'001,Policy::SequentialFull,opts);
 CHECK(normalized_result(r0)==normalized_result(r1));
 CHECK(!plain.last_run().resources.has_value());
 CHECK(plain.last_run().engine_measurements.empty());
 CHECK(profiled.last_run().resources.has_value());
 CHECK(profiled.last_run().engine_measurements.size()==2);
 CHECK(profiled.last_run().computations==2);
 CHECK(profiled.last_run().actual_compute_wall_sum_ns>=0);
 for(const auto& sample:profiled.last_run().engine_measurements){
   CHECK(sample.status==Status::Valid);
   CHECK(sample.id=="a"||sample.id=="b");
 }
 CHECK(profiled.last_run().resources->process_cpu_ns>=0);
 CHECK(profiled.last_run().resources->reserved_event_payload_bytes_lower_bound>0);
 // B1 measurements are attached to the actual worker, not process clock snapshots.
 Session parallel(graph);CHECK(parallel.store().ingest(event())==Ingest::Accepted);
 opts.parallel_grain_ns=1;opts.workers=2;
 const auto rp=parallel.execute(1'000'001,Policy::ParallelFull,opts);
 CHECK(normalized_result(rp)==normalized_result(r0));
 CHECK(parallel.last_run().engine_measurements.size()==2);
}
void test_frozen_costs(){
 Graph graph({descriptor("a"),descriptor("b")});
 Session incomplete(graph);
 PolicyOptions frozen;frozen.compute_budget_ns=1'000;frozen.cost_model=CostModel::FrozenCalibration;
 CHECK(incomplete.store().ingest(event())==Ingest::Accepted);
 throws([&]{(void)incomplete.execute(1'000'001,Policy::Freshness,frozen);});
 CHECK(incomplete.last_run().computations==0);
 throws([&]{incomplete.set_frozen_costs({{"a",1'000}});});
 throws([&]{incomplete.set_frozen_costs({{"a",1'000},{"b",0}});});
 throws([&]{incomplete.set_frozen_costs({{"a",1'000},{"stranger",2'000}});});
 incomplete.set_frozen_costs({{"a",1'000},{"b",5'000}});
 const auto scheduled=incomplete.execute(1'000'001,Policy::Freshness,frozen);
 CHECK(scheduled.at("a").status==Status::Valid);
 CHECK(scheduled.at("b").status!=Status::Valid);
 CHECK(incomplete.last_run().computations==1);
 CHECK(incomplete.last_run().estimated_cost_ns==1'000);
 CHECK(!scheduled.at("b").value.has_value());
 CHECK(incomplete.frozen_costs().size()==2);
 incomplete.reset();CHECK(incomplete.frozen_costs().size()==2); // replay reset preserves explicit frozen table
}
void test_file_validation(){
 Graph graph({descriptor("a"),descriptor("b")});
 const auto file=std::filesystem::temp_directory_path()/
  ("metalarch-m1-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tsv");
 auto write=[&](std::string body){std::ofstream o(file);o<<body;CHECK(static_cast<bool>(o));};
 write(table_header()+table_row("a")+table_row("b","rev1","6","5000"));
 const auto valid=load_frozen_cost_table(file.string(),graph);
 CHECK(valid.size()==2);CHECK(valid.at("a")==1'000);CHECK(valid.at("b")==5'000);
 write(table_header()+table_row("a"));throws([&]{(void)load_frozen_cost_table(file.string(),graph);});
 write(table_header()+table_row("a")+table_row("a"));throws([&]{(void)load_frozen_cost_table(file.string(),graph);});
 write(table_header()+table_row("a","wrong_revision")+table_row("b"));throws([&]{(void)load_frozen_cost_table(file.string(),graph);});
 auto changed=descriptor("a");changed.sources={Key{"fixture","DIFFERENT","1m",Kind::Bar}};
 Graph altered({changed,descriptor("b")});
 write(table_header()+table_row("a")+table_row("b"));
 throws([&]{(void)load_frozen_cost_table(file.string(),altered);});
 write(table_header()+table_row("a","rev1","4")+table_row("b"));throws([&]{(void)load_frozen_cost_table(file.string(),graph);});
 write(table_header()+table_row("a","rev1","5","0")+table_row("b"));throws([&]{(void)load_frozen_cost_table(file.string(),graph);});
 write("wrong header\n"+table_row("a")+table_row("b"));throws([&]{(void)load_frozen_cost_table(file.string(),graph);});
 std::filesystem::remove(file);
}
}
int main(){
 try{test_sampler();test_lazy_window_storage();test_opt_in();test_frozen_costs();test_file_validation();
   std::cout<<"PASS "<<checks<<" M1 instrumentation and frozen-calibration assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
