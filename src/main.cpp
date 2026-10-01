#include "metalarch/engines.hpp"
#include <fstream>
#include <iostream>
#include <string>
int main(int argc,char** argv){
 if(argc!=3 || std::string(argv[1])!="replay"){
   std::cerr<<"usage: metalarch_cli replay <trace.ma1>\n";return 2;
 }
 const ma::Key gold{"fixture","XAU","1m",ma::Kind::Bar},silver{"fixture","XAG","1m",ma::Kind::Bar},book{"fixture","XAU","live",ma::Kind::Book};
 ma::Session session(ma::make_metal_graph(gold,silver,book));
 std::ifstream input(argv[2]);if(!input){std::cerr<<"cannot read trace\n";return 2;}
 std::string line;size_t accepted=0,dups=0;
 try{
   while(std::getline(input,line)){
     if(line.empty()||line[0]=='#')continue;
     const auto e=ma::decode_event(line);const auto status=session.store().ingest(e);
     if(status==ma::Ingest::Accepted){++accepted;std::cout<<"event="<<accepted<<'|'<<ma::normalized_result(session.execute(e.ingest_ns));}
     else if(status==ma::Ingest::Duplicate)++dups;
     else throw std::runtime_error("invalid or out-of-order source event");
   }
 }catch(const std::exception& e){std::cerr<<"trace error: "<<e.what()<<'\n';return 1;}
 std::cerr<<"accepted="<<accepted<<" duplicates="<<dups<<" cache_hits="<<session.hits()<<" computes="<<session.computations()<<'\n';
 return 0;
}