#include "metalarch/trace.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <queue>
#include <set>
#include <memory>
#include <iterator>
#include <locale>
#include <random>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>
#include <cctype>
namespace ma {
namespace {
constexpr std::array<uint32_t,64> K={
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
uint32_t right(uint32_t v,unsigned n){return (v>>n)|(v<<(32U-n));}
std::vector<std::string> split(const std::string& text, char sep){
  std::vector<std::string> a;size_t start=0;
  for(size_t i=0;i<=text.size();++i)if(i==text.size()||text[i]==sep){a.push_back(text.substr(start,i-start));start=i+1;}
  return a;
}
void token(const std::string& t,const char* field){
  if(t.empty()||t.size()>160)throw std::invalid_argument(std::string(field)+" must be 1..160 characters");
  for(char letter:t){
    const auto c=static_cast<unsigned char>(letter);
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
        (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c==':'||c=='/'||c=='@'||c=='+'))
      throw std::invalid_argument(std::string(field)+" requires an ASCII safe-token; use an evidence ID, not free text");
  }
}
void validate_meta(const TraceMetadata& m){
  token(m.dataset_id,"dataset_id");token(m.origin,"origin");token(m.rights_ref,"rights_ref");
  if(m.origin!="USER_CSV" && m.origin!="CSV_BUNDLE" && m.origin!="MA1_MIGRATED" && m.origin!="SYNTHETIC_FIXTURE")
    throw std::invalid_argument("invalid origin; cannot relabel synthetic data as verified observed");
  if(m.origin=="CSV_BUNDLE" && (m.rights_ref.size()!=64 ||
      !std::all_of(m.rights_ref.begin(),m.rights_ref.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');})))
    throw std::invalid_argument("bundle metadata must bind the companion manifest SHA-256");
}
std::string header(const TraceMetadata& m){return "MA2|1|"+m.dataset_id+"|"+m.origin+"|"+m.rights_ref;}
std::string link_hash(const std::string& previous,uint64_t ordinal,const std::string& body){
  return sha256_hex(previous+"\n"+std::to_string(ordinal)+"\n"+body);
}
uint64_t unsigned_number(const std::string& s){
  uint64_t out=0;
  if(s.empty())throw std::invalid_argument("missing unsigned integer");
  auto [end,err]=std::from_chars(s.data(),s.data()+s.size(),out);
  if(err!=std::errc{}||end!=s.data()+s.size())throw std::invalid_argument("invalid unsigned integer");
  return out;
}
int64_t signed_number(const std::string& s){
  int64_t out=0;
  if(s.empty())throw std::invalid_argument("missing signed integer");
  auto [end,err]=std::from_chars(s.data(),s.data()+s.size(),out);
  if(err!=std::errc{}||end!=s.data()+s.size())throw std::invalid_argument("invalid signed integer");
  return out;
}
double real_number(const std::string& s){
  if(s.empty()||s.size()>96)throw std::invalid_argument("invalid numeric cell length");
  for(char letter:s){
    const auto c=static_cast<unsigned char>(letter);
    if(!((c>='0'&&c<='9')||c=='.'||c=='e'||c=='E'||c=='+'||c=='-'))
      throw std::invalid_argument("CSV numeric cells must contain finite ASCII decimal values only");
  }
  std::istringstream stream(s);stream.imbue(std::locale::classic());
  double v=0;stream>>v;
  if(!stream || stream.peek()!=std::char_traits<char>::eof() || !std::isfinite(v))
    throw std::invalid_argument("nonfinite/malformed CSV numeric cell");
  return v;
}
void count_mode(TraceSummary& s, Mode m){
  switch(m){case Mode::Observed:++s.observed;break;case Mode::Estimated:++s.estimated;break;
    case Mode::Simulated:++s.simulated;break;default:throw std::invalid_argument("unknown provenance mode");}
}
std::string next_line(std::ifstream& in){
  std::string line;
  if(!std::getline(in,line))throw std::runtime_error("MA2 trace missing END footer");
  if(line.empty()||line.size()>16384||line.back()=='\r')throw std::runtime_error("noncanonical/oversized MA2 line");
  return line;
}
}
std::string sha256_hex(std::string_view input){
  if(input.size()>((std::numeric_limits<uint64_t>::max()-72ULL)/8ULL))
    throw std::length_error("SHA-256 input length overflow");
  std::vector<uint8_t> v(input.begin(),input.end());
  const uint64_t bits=static_cast<uint64_t>(input.size())*8ULL;
  v.push_back(0x80);
  while(v.size()%64!=56)v.push_back(0);
  for(int b=7;b>=0;--b)v.push_back(static_cast<uint8_t>((bits>>(8*b))&0xff));
  std::array<uint32_t,8> state={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
    0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  for(size_t block=0;block<v.size();block+=64){
    uint32_t w[64]{};
    for(unsigned i=0;i<16;++i){const size_t q=block+i*4;
      w[i]=(static_cast<uint32_t>(v[q])<<24)|(static_cast<uint32_t>(v[q+1])<<16)|
           (static_cast<uint32_t>(v[q+2])<<8)|v[q+3];}
    for(unsigned i=16;i<64;++i){
      const auto a=w[i-15],b=w[i-2];
      const uint32_t s0=right(a,7)^right(a,18)^(a>>3);
      const uint32_t s1=right(b,17)^right(b,19)^(b>>10);
      w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    auto [a,b,c,d,e,f,g,h]=state;
    for(unsigned i=0;i<64;++i){
      const uint32_t s1=right(e,6)^right(e,11)^right(e,25);
      const uint32_t choose=(e&f)^(~e&g);
      const uint32_t t1=h+s1+choose+K[i]+w[i];
      const uint32_t s0=right(a,2)^right(a,13)^right(a,22);
      const uint32_t majority=(a&b)^(a&c)^(b&c);
      const uint32_t t2=s0+majority;
      h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;
    state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
  }
  std::ostringstream out;out<<std::hex<<std::setfill('0');
  for(uint32_t n:state)out<<std::setw(8)<<n;
  return out.str();
}
TraceWriter::TraceWriter(const std::filesystem::path& output,TraceMetadata metadata):
 destination_(output),metadata_(std::move(metadata)){
  validate_meta(metadata_);
  if(output.empty()||!std::filesystem::exists(output.parent_path().empty()?".":output.parent_path()))
    throw std::invalid_argument("trace output parent directory must exist");
  if(std::filesystem::exists(destination_))throw std::runtime_error("refusing to overwrite an existing trace");
  const auto now=std::chrono::steady_clock::now().time_since_epoch().count();
  temp_=output;
  temp_+=".partial."+std::to_string(now)+"."+std::to_string(std::random_device{}());
  if(std::filesystem::exists(temp_))throw std::runtime_error("temporary trace name collision");
  out_.open(temp_,std::ios::binary|std::ios::trunc);
  if(!out_)throw std::runtime_error("cannot create temporary trace");
  auto h=header(metadata_);
  root_=sha256_hex("MetalArch-MA2-v1\n"+h);
  out_<<h<<'\n';
  if(!out_)throw std::runtime_error("cannot write trace header");
}
TraceWriter::~TraceWriter(){
  if(out_.is_open())out_.close();
  if(!finished_ && !temp_.empty()){
    std::error_code err;std::filesystem::remove(temp_,err);
  }
}
Ingest TraceWriter::append(const Event& event){
  if(finished_)throw std::logic_error("recording already finalized");
  if(global_ingest_ns_>event.ingest_ns)throw std::runtime_error("global ingestion time inversion");
  const auto code=validation_.ingest(event);
  if(code!=Ingest::Accepted && code!=Ingest::Duplicate)
    throw std::runtime_error("invalid, conflicting or out-of-order input event");
  // Hash both accepted events and discarded duplicates. The latter are audited
  // explicitly as D rows: otherwise a writer-supplied duplicate total is unverifiable.
  const std::string body=encode_event(event);
  const uint64_t ordinal=summary_.accepted+summary_.duplicates+1;
  const std::string new_root=link_hash(root_,ordinal,body);
  out_<<(code==Ingest::Accepted?"E|":"D|")<<ordinal<<'|'<<new_root<<'|'<<body<<'\n';
  if(!out_)throw std::runtime_error("recording write failure");
  root_=new_root;
  global_ingest_ns_=event.ingest_ns;
  if(code==Ingest::Duplicate)++summary_.duplicates;
  else {++summary_.accepted;count_mode(summary_,event.mode);}
  return code;
}
TraceSummary TraceWriter::finish(){
  if(finished_)throw std::logic_error("recording already finalized");
  if(summary_.accepted==0)throw std::runtime_error("empty recordings are not accepted");
  out_<<"END|"<<summary_.accepted<<'|'<<summary_.duplicates<<'|'<<root_<<'\n';
  out_.flush();if(!out_)throw std::runtime_error("cannot finalize trace");
  out_.close();if(!out_)throw std::runtime_error("cannot close finalized trace");
  // Same-directory hard-link install is exclusive: unlike rename(), does not overwrite an existing destination.
  std::filesystem::create_hard_link(temp_,destination_);
  std::filesystem::remove(temp_);finished_=true;
  summary_.root_sha256=root_;
  return summary_;
}
TraceReader::TraceReader(const std::filesystem::path& file):in_(file,std::ios::binary){
  if(!in_)throw std::runtime_error("cannot open MA2 recording");
  const auto line=next_line(in_);
  const auto fields=split(line,'|');
  if(fields.size()!=5 || fields[0]!="MA2" || fields[1]!="1")throw std::runtime_error("invalid MA2 metadata header");
  metadata_={fields[2],fields[3],fields[4]};validate_meta(metadata_);
  if(header(metadata_)!=line)throw std::runtime_error("noncanonical MA2 header");
  root_=sha256_hex("MetalArch-MA2-v1\n"+line);
}
bool TraceReader::next(Event& event){
  if(finished_)return false;
  for(;;){
    const std::string line=next_line(in_);
    if(line.rfind("END|",0)==0){
      const auto f=split(line,'|');
      if(f.size()!=4||unsigned_number(f[1])!=summary_.accepted||
         unsigned_number(f[2])!=summary_.duplicates||f[3]!=root_)
        throw std::runtime_error("MA2 footer count/root mismatch");
      if(summary_.accepted==0)throw std::runtime_error("empty MA2 recording");
      if(in_.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing content after MA2 footer");
      summary_.root_sha256=root_;finished_=true;return false;
    }
    const bool duplicate=line.rfind("D|",0)==0;
    if(!duplicate && line.rfind("E|",0)!=0)throw std::runtime_error("unknown MA2 record");
    const auto first=line.find('|',2);
    const auto second=first==std::string::npos?std::string::npos:line.find('|',first+1);
    if(first==std::string::npos||second==std::string::npos)throw std::runtime_error("malformed MA2 event row");
    const uint64_t ordinal=unsigned_number(line.substr(2,first-2));
    const std::string digest=line.substr(first+1,second-first-1);
    const std::string body=line.substr(second+1);
    if(ordinal!=summary_.accepted+summary_.duplicates+1 || digest.size()!=64 ||
       digest!=link_hash(root_,ordinal,body))throw std::runtime_error("MA2 event ordinal/hash-chain mismatch");
    event=decode_event(body);
    if(encode_event(event)!=body)throw std::runtime_error("noncanonical MA2 payload encoding");
    if(global_ingest_ns_>event.ingest_ns)throw std::runtime_error("MA2 input has inverted global ingestion time");
    const auto code=validation_.ingest(event);
    if(code!=(duplicate?Ingest::Duplicate:Ingest::Accepted))
      throw std::runtime_error("MA2 E/D row conflicts with independently checked ingestion status");
    global_ingest_ns_=event.ingest_ns;
    root_=digest;
    if(duplicate){++summary_.duplicates;continue;}
    ++summary_.accepted;count_mode(summary_,event.mode);return true;
  }
}
const TraceSummary& TraceReader::summary() const{
  if(!finished_)throw std::logic_error("MA2 integrity is unverified until footer and EOF are read");
  return summary_;
}
TraceSummary pack_ma1(const std::filesystem::path& path,const std::filesystem::path& output,TraceMetadata m){
  if(m.origin!="MA1_MIGRATED"&&m.origin!="SYNTHETIC_FIXTURE")throw std::invalid_argument("MA1 pack origin must disclose prior encoding");
  std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("cannot read input MA1");
  TraceWriter writer(output,std::move(m));
  std::string line;size_t number=0;
  while(std::getline(input,line)){
    ++number;
    if(line.empty()||line[0]=='#')continue;
    if(line.size()>8192||line.back()=='\r')throw std::runtime_error("malformed MA1 at line "+std::to_string(number));
    writer.append(decode_event(line));
  }
  if(input.bad())throw std::runtime_error("MA1 source read error");
  return writer.finish();
}
namespace {
Mode parse_csv_mode(const std::string& mode){
  if(mode=="observed")return Mode::Observed;
  if(mode=="estimated")return Mode::Estimated;
  if(mode=="simulated")return Mode::Simulated;
  throw std::invalid_argument("invalid CSV provenance; choose observed|estimated|simulated");
}
Kind parse_csv_kind(const std::string& kind){
  if(kind=="bar")return Kind::Bar;
  if(kind=="book")return Kind::Book;
  throw std::invalid_argument("invalid CSV kind; choose bar|book");
}
class CsvCursor {
public:
  CsvCursor(const std::filesystem::path& path,Key key,Mode mode):input_(path,std::ios::binary),key_(std::move(key)),mode_(mode){
    if(!input_)throw std::runtime_error("cannot read source CSV: "+path.string());
    const auto cols=split(next_line(input_),',');
    const std::vector<std::string> bars={"seq","event_ns","ingest_ns","open","high","low","close","volume"};
    const std::vector<std::string> book={"seq","event_ns","ingest_ns","bid","ask","bid_qty","ask_qty"};
    if((key_.kind==Kind::Bar&&cols!=bars)||(key_.kind==Kind::Book&&cols!=book))
      throw std::invalid_argument("incorrect source CSV schema: "+path.string());
    fields_=cols.size();advance();
  }
  bool has_next() const {return available_;}
  const Event& current() const {if(!available_)throw std::logic_error("CSV cursor exhausted");return next_;}
  void advance(){
    std::string line;
    if(!std::getline(input_,line)){
      if(input_.bad())throw std::runtime_error("source CSV read error");
      available_=false;return;
    }
    ++row_;
    if(line.empty()||line.size()>8192||line.back()=='\r'||line.find('"')!=std::string::npos)
      throw std::invalid_argument("noncanonical source CSV at data row "+std::to_string(row_));
    const auto cells=split(line,',');
    if(cells.size()!=fields_)throw std::invalid_argument("source CSV column count mismatch at row "+std::to_string(row_));
    try{
      Event e;e.key=key_;e.mode=mode_;e.seq=unsigned_number(cells[0]);
      e.event_ns=signed_number(cells[1]);e.ingest_ns=signed_number(cells[2]);
      e.a=real_number(cells[3]);e.b=real_number(cells[4]);e.c=real_number(cells[5]);
      e.d=real_number(cells[6]);e.e=key_.kind==Kind::Bar?real_number(cells[7]):0;
      if(previous_ingest_>e.ingest_ns)throw std::invalid_argument("per-feed ingestion time inversion");
      previous_ingest_=e.ingest_ns;next_=std::move(e);available_=true;
    }catch(const std::exception& e){throw std::runtime_error("source CSV row "+std::to_string(row_)+": "+e.what());}
  }
private:
  std::ifstream input_;
  Key key_;
  Mode mode_;
  size_t fields_{0},row_{0};
  int64_t previous_ingest_{0};
  bool available_{false};
  Event next_;
};
}
TraceSummary record_csv(const std::filesystem::path& path,const std::filesystem::path& output,
                        TraceMetadata m,const Key& key,Mode mode){
  if(m.origin!="USER_CSV")throw std::invalid_argument("CSV input origin must be USER_CSV");
  if(m.rights_ref=="unknown"||m.rights_ref=="unverified"||m.rights_ref=="none")
    throw std::invalid_argument("USER_CSV requires a non-placeholder, user-supplied evidence reference");
  if(static_cast<uint8_t>(mode)>static_cast<uint8_t>(Mode::Simulated))throw std::invalid_argument("bad requested provenance mode");
  CsvCursor cursor(path,key,mode);
  TraceWriter writer(output,std::move(m));
  while(cursor.has_next()){writer.append(cursor.current());cursor.advance();}
  return writer.finish();
}
TraceSummary record_csv_bundle(const std::filesystem::path& manifest,
                                const std::filesystem::path& output,const std::string& dataset_id){
  std::ifstream input(manifest,std::ios::binary);
  if(!input)throw std::runtime_error("cannot read bundle source manifest");
  const std::string raw((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
  if(raw.empty()||raw.size()>131072)throw std::invalid_argument("invalid/oversized CSV bundle manifest");
  std::istringstream registry(raw);registry.imbue(std::locale::classic());
  std::string line;
  if(!std::getline(registry,line)||line!="source\tsymbol\ttimeframe\tkind\tmode\tcsv_path\trights_ref")
    throw std::invalid_argument("CSV bundle requires exact seven-column TSV header");
  std::vector<std::unique_ptr<CsvCursor>> streams;
  std::set<Key> seen;
  while(std::getline(registry,line)){
    if(line.empty()||line.back()=='\r'||streams.size()>=256)
      throw std::invalid_argument("blank/noncanonical/oversized bundle manifest");
    const auto cols=split(line,'\t');
    if(cols.size()!=7)throw std::invalid_argument("bundle manifest must declare 7 fields per stream");
    token(cols[0],"source");token(cols[1],"symbol");token(cols[2],"timeframe");
    token(cols[6],"per-source rights_ref");
    if(cols[6]=="unknown"||cols[6]=="none"||cols[6]=="unverified")
      throw std::invalid_argument("placeholder rights_ref is not permitted in a source bundle");
    if(cols[5].empty()||cols[5].find('\0')!=std::string::npos)
      throw std::invalid_argument("bundle source CSV path is required");
    const Key key{cols[0],cols[1],cols[2],parse_csv_kind(cols[3])};
    if(!seen.insert(key).second)throw std::invalid_argument("duplicate source identity in bundle manifest");
    std::filesystem::path filepath(cols[5]);
    if(filepath.is_relative())filepath=manifest.parent_path()/filepath;
    streams.emplace_back(std::make_unique<CsvCursor>(filepath,key,parse_csv_mode(cols[4])));
  }
  if(streams.empty())throw std::invalid_argument("source bundle manifest is empty");
  TraceWriter writer(output,{dataset_id,"CSV_BUNDLE",sha256_hex(raw)});
  struct Item{int64_t ingest;size_t stream;};
  auto greater=[](const Item& x,const Item& y){
    return x.ingest!=y.ingest?x.ingest>y.ingest:x.stream>y.stream;
  };
  std::priority_queue<Item,std::vector<Item>,decltype(greater)> queue(greater);
  for(size_t i=0;i<streams.size();++i)if(streams[i]->has_next())
    queue.push({streams[i]->current().ingest_ns,i});
  while(!queue.empty()){
    const auto item=queue.top();queue.pop();
    writer.append(streams[item.stream]->current());
    streams[item.stream]->advance();
    if(streams[item.stream]->has_next())
      queue.push({streams[item.stream]->current().ingest_ns,item.stream});
  }
  return writer.finish();
}
}
