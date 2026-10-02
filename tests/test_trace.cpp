#include "metalarch/trace.hpp"
#include "metalarch/engines.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
using namespace ma;
namespace {
size_t checks=0;
#define CHECK(c) do {++checks;if(!(c))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " + #c);}while(0)
template<class F>void throws(F&& f){bool yes=false;try{f();}catch(const std::exception&){yes=true;}CHECK(yes);}
const Key gold{"fixture","XAU","1m",Kind::Bar};
Event row(uint64_t seq,int64_t ingest_increment=0){
  const int64_t t=1'000'000'000LL+static_cast<int64_t>(seq)*60'000'000LL;
  return {gold,seq,t,t+1000+ingest_increment,100,102,99,101,100,Mode::Simulated};
}
std::string text(const std::filesystem::path& path){
  std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};
}
void write(const std::filesystem::path& path,const std::string& s){
  std::ofstream f(path,std::ios::binary|std::ios::trunc);f<<s;CHECK(static_cast<bool>(f));
}
std::string root(const std::filesystem::path& path){
  TraceReader r(path);Event e;while(r.next(e)){}return r.summary().root_sha256;
}
void verify_fails(const std::filesystem::path& path,const std::string& data){
  write(path,data);throws([&]{(void)root(path);});
}
void test_vectors(){
 CHECK(sha256_hex("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
 CHECK(sha256_hex("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 CHECK(sha256_hex(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
void test_integrity(const std::filesystem::path& dir){
  auto path=dir/"valid.ma2";
  TraceWriter writer(path,{"demo","SYNTHETIC_FIXTURE","synthetic.fixture"});
  CHECK(writer.append(row(1))==Ingest::Accepted);
  CHECK(writer.append(row(1))==Ingest::Duplicate);
  CHECK(writer.append(row(2))==Ingest::Accepted);
  const auto written=writer.finish();
  CHECK(written.accepted==2);CHECK(written.duplicates==1);CHECK(written.simulated==2);
  CHECK(written.root_sha256.size()==64);CHECK(root(path)==written.root_sha256);
  TraceReader r(path);Event e;throws([&]{(void)r.summary();});
  CHECK(r.next(e));CHECK(encode_event(e)==encode_event(row(1)));
  CHECK(r.next(e));CHECK(e.seq==2);CHECK(!r.next(e));CHECK(!r.next(e));
  CHECK(r.summary().accepted==2);CHECK(r.summary().duplicates==1);
  throws([&]{TraceWriter again(path,{"demo","SYNTHETIC_FIXTURE","synthetic.fixture"});});
  CHECK(root(path)==written.root_sha256);
  auto original=text(path);
  auto tmp=dir/"damaged.ma2";
  {
    auto tampered=original;const auto where=tampered.find("101|");CHECK(where!=std::string::npos);
    tampered[where]='9';verify_fails(tmp,tampered);
  }
  {auto tampered=original;auto q=tampered.find("E|3|");CHECK(q!=std::string::npos);
   tampered.erase(q,tampered.find('\n',q)-q+1);verify_fails(tmp,tampered);}
  {auto tampered=original;auto q=tampered.find("END|");CHECK(q!=std::string::npos);
   tampered.erase(q);verify_fails(tmp,tampered);}
  verify_fails(tmp,original+"E|extra\n");
  {auto tampered=original;auto q=tampered.find("demo");CHECK(q!=std::string::npos);
   tampered.replace(q,4,"deMO");verify_fails(tmp,tampered);}
  {auto tampered=original;auto q=tampered.find("E|3|");CHECK(q!=std::string::npos);
   tampered[q+2]='1';verify_fails(tmp,tampered);}
  // D/E status is verified by independent Store semantics, not accepted as metadata.
  {auto tampered=original;auto q=tampered.find("D|2|");CHECK(q!=std::string::npos);
   tampered[q]='E';verify_fails(tmp,tampered);}
  CHECK(root(path)==written.root_sha256);
}
void test_reject_partial(const std::filesystem::path& dir){
  auto file=dir/"reject.ma2";
  {
   TraceWriter writer(file,{"bad","SYNTHETIC_FIXTURE","fixture"});
   writer.append(row(1));
   throws([&]{writer.append(row(1,5));});
   CHECK(!std::filesystem::exists(file));
  }
  CHECK(!std::filesystem::exists(file));
  {
   TraceWriter writer(file,{"bad","SYNTHETIC_FIXTURE","fixture"});
   writer.append(row(2));
   throws([&]{writer.append(row(1));});
  }
  CHECK(!std::filesystem::exists(file));
  throws([&]{TraceWriter w(file,{"bad|hello","USER_CSV","fixture"});});
  throws([&]{TraceWriter w(file,{"bad","UNKNOWN_ORIGIN","fixture"});});
}
void test_csv(const std::filesystem::path& dir){
  const auto csv=dir/"data.csv", trace=dir/"csv.ma2";
  write(csv,"seq,event_ns,ingest_ns,open,high,low,close,volume\n"
        "1,1000000000,1000001000,100,102,99,101,120\n"
        "2,2000000000,2000001000,101,104,100,103,150\n");
  auto meta=TraceMetadata{"research_subset","USER_CSV","owner.agreement.2026"};
  const Key key{"user.dataset","XAU","1m",Kind::Bar};
  auto s=record_csv(csv,trace,meta,key,Mode::Observed);
  CHECK(s.accepted==2);CHECK(s.observed==2);CHECK(s.simulated==0);
  TraceReader r(trace);Event e;CHECK(r.next(e));CHECK(e.key==key);
  CHECK(e.mode==Mode::Observed);CHECK(e.e==120);CHECK(r.next(e));CHECK(e.d==103);
  CHECK(!r.next(e));CHECK(r.summary().root_sha256==s.root_sha256);
  // Evidence reference is user-supplied, not provider authenticated; never invent one.
  throws([&]{(void)record_csv(csv,dir/"invalid.ma2",{"x","USER_CSV","unknown"},key,Mode::Observed);});
  write(csv,"seq,event_ns,ingest_ns,open,high,low,close,volume\n"
            "1,1000000000,1000001000,100,102,99,101,120\n"
            "2,2000000000,2000001000,101,104,100,NaN,150\n");
  throws([&]{(void)record_csv(csv,dir/"failure.ma2",meta,key,Mode::Observed);});
  CHECK(!std::filesystem::exists(dir/"failure.ma2"));
  write(csv,"seq,event_ns,ingest_ns,open,high,low,close,volume\n"
            "1,1000000000,1000001000,100,102,99,101,120\n"
            "2,2000000000,1999999999,101,104,100,103,150\n");
  throws([&]{(void)record_csv(csv,dir/"future.ma2",meta,key,Mode::Observed);});
  CHECK(!std::filesystem::exists(dir/"future.ma2"));
  write(csv,"seq,event_ns,ingest_ns,bid,ask,bid_qty,ask_qty\n"
            "1,1000000000,1000001000,99,101,20,30\n");
  const Key book{"user.dataset","XAU","live",Kind::Book};
  auto t=record_csv(csv,dir/"book.ma2",meta,book,Mode::Estimated);
  CHECK(t.estimated==1);CHECK(t.accepted==1);
  TraceReader b(dir/"book.ma2");CHECK(b.next(e));CHECK(e.key.kind==Kind::Book);
  CHECK(e.mode==Mode::Estimated);CHECK(!b.next(e));
}
void test_bundle(const std::filesystem::path& dir,const std::filesystem::path& manifest,
                 const std::filesystem::path& ma1){
  const auto out=dir/"bundle.ma2";
  auto summary=record_csv_bundle(manifest,out,"three_streams");
  CHECK(summary.accepted==210);CHECK(summary.duplicates==0);CHECK(summary.simulated==210);
  CHECK(root(out)==summary.root_sha256);
  TraceReader r(out);CHECK(r.metadata().origin=="CSV_BUNDLE");
  CHECK(r.metadata().rights_ref==sha256_hex(text(manifest)));
  Event item;std::ifstream reference(ma1);std::string line;
  std::vector<Event> expected;
  while(std::getline(reference,line))if(!line.empty()&&line[0]!='#')expected.push_back(decode_event(line));
  size_t pos=0;
  while(r.next(item)){
    CHECK(pos<expected.size());CHECK(encode_event(item)==encode_event(expected[pos]));++pos;
  }
  CHECK(pos==210);CHECK(r.summary().root_sha256==summary.root_sha256);
  // A source manifest must be immutable relative to the SHA embedded in the MA2 header.
  const auto modified=text(manifest)+"# changed rights metadata\n";
  CHECK(sha256_hex(modified)!=r.metadata().rights_ref);
  // A registry may never alias the same source key to two different CSV cursors.
  const auto duplicate_manifest=dir/"duplicate-source.tsv";
  auto first=text(manifest);auto nl=first.find('\n');CHECK(nl!=std::string::npos);
  auto second=first.find('\n',nl+1);CHECK(second!=std::string::npos);
  // Replace relative paths with absolute paths so this fixture can move to the scratch directory.
  auto replace_all=[&](const std::string& value,const std::string& search,const std::string& replacement){
     std::string out=value;size_t p=0;
     while((p=out.find(search,p))!=std::string::npos){out.replace(p,search.size(),replacement);p+=replacement.size();}
     return out;
  };
  for(const auto& name:{"gold_bars.csv","silver_bars.csv","gold_book.csv"})
    first=replace_all(first,name,(manifest.parent_path()/name).string());
  auto line1_abs=first.substr(first.find('\n')+1,first.find('\n',first.find('\n')+1)-first.find('\n'));
  first+=line1_abs;
  write(duplicate_manifest,first);
  throws([&]{(void)record_csv_bundle(duplicate_manifest,dir/"duplicate-output.ma2","duplicate");});
  CHECK(!std::filesystem::exists(dir/"duplicate-output.ma2"));
}
void test_ma1_equivalence(const std::filesystem::path& dir,const std::filesystem::path& original){
  auto filename=dir/"from-ma1.ma2";
  const auto result=pack_ma1(original,filename,{"converted","MA1_MIGRATED","synthetic.fixture"});
  CHECK(result.accepted==210);CHECK(result.simulated==210);
  std::vector<Event> direct;std::ifstream input(original);std::string line;
  while(std::getline(input,line))if(!line.empty()&&line[0]!='#')direct.push_back(decode_event(line));
  CHECK(direct.size()==210);
  TraceReader r(filename);Event e;size_t index=0;
  while(r.next(e)){CHECK(index<direct.size());CHECK(encode_event(e)==encode_event(direct[index]));++index;}
  CHECK(index==direct.size());CHECK(r.summary().root_sha256==result.root_sha256);
}
}
int main(int argc,char**argv){
 try{
  if(argc!=4)throw std::invalid_argument("usage test_trace <scratch-dir> <fixture.ma1> <bundle-manifest.tsv>");
  const auto scratch=std::filesystem::path(argv[1]);
  if(std::filesystem::exists(scratch))std::filesystem::remove_all(scratch);
  std::filesystem::create_directories(scratch);
  test_vectors();test_integrity(scratch);test_reject_partial(scratch);test_csv(scratch);test_ma1_equivalence(scratch,argv[2]);test_bundle(scratch,argv[3],argv[2]);
  std::filesystem::remove_all(scratch);
  std::cout<<"PASS "<<checks<<" MA2 SHA-256, integrity, CSV adapter and deterministic-replay assertions\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
