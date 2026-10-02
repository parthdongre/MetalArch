#include "metalarch/resource.hpp"
#include "metalarch/core.hpp"
#include <cmath>
#include <ctime>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
# include <sys/resource.h>
# include <time.h>
# include <unistd.h>
#endif
#if defined(__APPLE__)
# include <mach/mach.h>
# include <mach/task_info.h>
#endif
namespace ma {
namespace {
constexpr uint64_t HASH_INIT=14695981039346656037ULL,HASH_PRIME=1099511628211ULL;
void huint(uint64_t& h,uint64_t x){for(int i=0;i<8;++i){h^=(x&255);h*=HASH_PRIME;x>>=8;}}
void hstr(uint64_t& h,const std::string& s){
 huint(h,s.size());
 for(char raw:s){h^=static_cast<uint64_t>(static_cast<unsigned char>(raw));h*=HASH_PRIME;}
}
uint64_t u64(const std::string& s){
 if(s.empty() || s[0]=='-')throw std::invalid_argument("invalid calibration unsigned value");
 size_t end=0;uint64_t v=std::stoull(s,&end);
 if(end!=s.size())throw std::invalid_argument("invalid calibration unsigned value");
 return v;
}
std::vector<std::string> fields(const std::string& line){
 std::vector<std::string> out;size_t pos=0;
 while(true){const size_t end=line.find('\t',pos);
   out.push_back(line.substr(pos,end==std::string::npos?end:end-pos));
   if(end==std::string::npos)break;
   pos=end+1;
 }
 return out;
}
}
uint64_t descriptor_signature(const Descriptor& d){
 uint64_t h=HASH_INIT;hstr(h,d.id);hstr(h,d.revision);hstr(h,d.parameters);
 huint(h,static_cast<uint64_t>(d.max_source_age_ns));
 huint(h,static_cast<uint64_t>(d.max_result_age_ns));huint(h,d.cadence);
 huint(h,d.sources.size());
 for(const auto& k:d.sources){hstr(h,k.source);hstr(h,k.symbol);hstr(h,k.timeframe);huint(h,static_cast<uint64_t>(k.kind));}
 huint(h,d.dependencies.size());for(const auto& p:d.dependencies)hstr(h,p);
 return h;
}
ResourceSnapshot sample_process_resources(){
 ResourceSnapshot o;
 const std::clock_t c=std::clock();
 if(c!=static_cast<std::clock_t>(-1)){
   const long double n=static_cast<long double>(c)*1'000'000'000.0L/static_cast<long double>(CLOCKS_PER_SEC);
   if(n>=0 && n<=static_cast<long double>(std::numeric_limits<uint64_t>::max()))o.process_cpu_ns=static_cast<uint64_t>(n);
 }
#if defined(__APPLE__) || (defined(__unix__) && !defined(__linux__))
 struct rusage ru{};
 if(getrusage(RUSAGE_SELF,&ru)==0){
#if defined(__APPLE__)
   // macOS reports bytes; Linux reports KiB.
   o.peak_rss_bytes=static_cast<uint64_t>(ru.ru_maxrss);
   o.peak_rss_supported=true;
#else
   o.peak_rss_bytes=static_cast<uint64_t>(ru.ru_maxrss)*1024ULL;
   o.peak_rss_supported=true;
#endif
 }
#endif
#if defined(__linux__)
 // Linux getrusage(ru_maxrss) can inherit a fork/spawn parent's high-water
 // mark even after exec. VmHWM belongs to this executable's fresh mm and
 // avoids attributing the Python experiment launcher's RSS to MetalArch.
 std::ifstream status("/proc/self/status");std::string status_line;
 while(std::getline(status,status_line)){
   if(status_line.rfind("VmHWM:",0)==0){
     std::istringstream value(status_line.substr(6));uint64_t kib=0;
     if(value>>kib && kib<=UINT64_MAX/1024ULL){
       o.peak_rss_bytes=kib*1024ULL;o.peak_rss_supported=true;
     }
     break;
   }
 }
 std::ifstream in("/proc/self/statm");uint64_t virtual_pages=0,resident_pages=0;
 if(in>>virtual_pages>>resident_pages){
   const long n=sysconf(_SC_PAGESIZE);
   if(n>0 && resident_pages<=UINT64_MAX/static_cast<uint64_t>(n)){
     o.current_rss_bytes=resident_pages*static_cast<uint64_t>(n);o.current_rss_supported=true;
   }
 }
#elif defined(__APPLE__)
 mach_task_basic_info_data_t info{};mach_msg_type_number_t count=MACH_TASK_BASIC_INFO_COUNT;
 if(task_info(mach_task_self(),MACH_TASK_BASIC_INFO,reinterpret_cast<task_info_t>(&info),&count)==KERN_SUCCESS){
   o.current_rss_bytes=static_cast<uint64_t>(info.resident_size);o.current_rss_supported=true;
 }
#endif
 if(o.current_rss_supported && o.peak_rss_supported)
   o.peak_rss_bytes=std::max(o.peak_rss_bytes,o.current_rss_bytes);
 return o;
}
std::optional<uint64_t> sample_thread_cpu_ns(){
#if defined(CLOCK_THREAD_CPUTIME_ID)
 struct timespec ts{};
 if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&ts)==0 && ts.tv_sec>=0 && ts.tv_nsec>=0)
   return static_cast<uint64_t>(ts.tv_sec)*1'000'000'000ULL+static_cast<uint64_t>(ts.tv_nsec);
#endif
 return std::nullopt;
}
std::map<std::string,uint64_t> load_frozen_cost_table(const std::string& path,const Graph& graph){
 std::ifstream in(path);if(!in)throw std::runtime_error("cannot open calibration table: "+path);
 std::string line;
 if(!std::getline(in,line) || line!="id\trevision\tparameters\tdescriptor_digest\tvalid_samples\twall_p50_ns\twall_p95_ns\tthread_cpu_p95_ns\tcost_ns")
   throw std::invalid_argument("invalid calibration schema");
 std::map<std::string,const Descriptor*> expected;
 for(const auto& d:graph.sorted())expected.emplace(d.id,&d);
 std::map<std::string,uint64_t> table;
 while(std::getline(in,line)){
   if(line.empty())continue;
   const auto p=fields(line);if(p.size()!=9)throw std::invalid_argument("invalid calibration row");
   const auto it=expected.find(p[0]);
   if(it==expected.end() || it->second->revision!=p[1] || it->second->parameters!=p[2])
     throw std::invalid_argument("calibration identity mismatch: "+p[0]);
   if(u64(p[3])!=descriptor_signature(*it->second))throw std::invalid_argument("calibration descriptor digest mismatch: "+p[0]);
   const uint64_t samples=u64(p[4]),p50=u64(p[5]),p95=u64(p[6]);(void)u64(p[7]);
   const uint64_t cost=u64(p[8]);
   if(samples<5||p50==0||p95<p50||cost==0||!table.emplace(p[0],cost).second)
     throw std::invalid_argument("empty, zero-cost or duplicate calibration row");
 }
 if(table.size()!=expected.size())throw std::invalid_argument("calibration must contain all graph engines");
 return table;
}
}
