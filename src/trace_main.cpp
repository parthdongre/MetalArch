#include "metalarch/trace.hpp"
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
namespace {
ma::Mode parse_mode(const std::string& mode){
  if(mode=="observed")return ma::Mode::Observed;
  if(mode=="estimated")return ma::Mode::Estimated;
  if(mode=="simulated")return ma::Mode::Simulated;
  throw std::invalid_argument("mode must be observed, estimated or simulated");
}
void show(const ma::TraceSummary& s){
  std::cout<<"schema=MA2 accepted="<<s.accepted<<" duplicates="<<s.duplicates
           <<" observed="<<s.observed<<" estimated="<<s.estimated
           <<" simulated="<<s.simulated<<" root_sha256="<<s.root_sha256<<'\n';
}
}
int main(int argc,char** argv){
  try{
    if(argc>=2 && std::string(argv[1])=="verify" && (argc==3||argc==4||argc==5)){
      ma::TraceReader reader(argv[2]);ma::Event e;
      while(reader.next(e)){}
      if(argc>=4 && std::string(argv[3])!=reader.summary().root_sha256)
        throw std::runtime_error("verified recording root differs from independent expected root");
      if(argc==5){
        if(reader.metadata().origin!="CSV_BUNDLE")
          throw std::runtime_error("source-manifest verification applies only to CSV_BUNDLE traces");
        std::ifstream manifest(argv[4],std::ios::binary);
        if(!manifest)throw std::runtime_error("cannot read accompanying source manifest");
        const std::string bytes((std::istreambuf_iterator<char>(manifest)),std::istreambuf_iterator<char>());
        if(ma::sha256_hex(bytes)!=reader.metadata().rights_ref)
          throw std::runtime_error("source manifest digest differs from MA2 binding");
      }
      std::cout<<"dataset="<<reader.metadata().dataset_id<<" origin="<<reader.metadata().origin
               <<" rights_ref="<<reader.metadata().rights_ref<<' ';
      show(reader.summary());return 0;
    }
    if(argc>=2 && std::string(argv[1])=="pack-ma1" && argc==6){
      show(ma::pack_ma1(argv[2],argv[3],{argv[4],"MA1_MIGRATED",argv[5]}));return 0;
    }
    if(argc>=2 && std::string(argv[1])=="record-bundle" && argc==5){
      show(ma::record_csv_bundle(argv[2],argv[3],argv[4]));
      std::cerr<<"BUNDLE rights/provenance are user-declared and the MA2 header pins the companion manifest SHA-256.\n";
      return 0;
    }
    if(argc>=2 && std::string(argv[1])=="record-csv" && argc==11){
      const std::string kind=argv[9];
      if(kind!="bar"&&kind!="book")throw std::invalid_argument("kind must be bar or book");
      ma::Key key{argv[6],argv[7],argv[8],kind=="bar"?ma::Kind::Bar:ma::Kind::Book};
      show(ma::record_csv(argv[2],argv[3],{argv[4],"USER_CSV",argv[5]},key,parse_mode(argv[10])));
      std::cerr<<"USER_CSV source/rights/provenance are user assertions, not independently authenticated or licensed by MetalArch.\n";
      return 0;
    }
    std::cerr<<"usage:\n"
      <<"  metalarch_trace verify <trace.ma2> [independently-retained-root-sha256] [source-manifest.tsv]\n"
      <<"  metalarch_trace pack-ma1 <input.ma1> <output.ma2> <dataset-id> <rights-ref>\n"
      <<"  metalarch_trace record-bundle <source-manifest.tsv> <output.ma2> <dataset-id>\n"
      <<"  metalarch_trace record-csv <input.csv> <output.ma2> <dataset-id> <rights-ref>"
        " <source> <symbol> <timeframe> <bar|book> <observed|estimated|simulated>\n";
    return 2;
  }catch(const std::exception& e){std::cerr<<"recording error: "<<e.what()<<'\n';return 1;}
}
