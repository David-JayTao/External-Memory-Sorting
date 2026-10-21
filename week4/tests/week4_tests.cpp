#include "external/sort.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace external;
namespace {
void check(bool x,const char* message){if(!x)throw std::runtime_error(message);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"expected rejection");}
struct Sandbox {
 std::filesystem::path path=std::filesystem::temp_directory_path()/("week4-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Sandbox(){if(!std::filesystem::create_directory(path))throw std::runtime_error("sandbox collision");}
 ~Sandbox(){std::error_code ec;std::filesystem::remove_all(path,ec);}
};
}
int main(){try{
 Sandbox box; IOStats s;std::mt19937 rng(42);int cases=0;
 for(std::size_t n:{0U,1U,2U,15U,16U,17U,101U,1003U})for(std::size_t b:{1U,3U,16U}){
    const auto dir=box.path/std::to_string(cases++);std::filesystem::create_directory(dir);
    std::vector<Value> oracle(n);for(auto&v:oracle)v=static_cast<Value>(rng()%201)-100;
    generate_raw(dir/"raw",n,[&](std::uint64_t i){return oracle[i];},b,s);
    s={};RunGeneration(dir/"raw",dir/"initial",16,b,s);
    check(s.data_read_bytes==4*n&&s.data_write_bytes==4*n,"generation byte conservation");
    check(s.data_reads==(n+b-1)/b,"generation read block count");
    std::uint64_t expectedWrites=0;for(std::size_t i=0;i<n;i+=16)expectedWrites+=(std::min<std::size_t>(16,n-i)+b-1)/b;
    check(s.data_writes==expectedWrites,"generation write block count");
    {RunFileReader r(dir/"initial",s);check(r.run_count()==(n+15)/16,"initial run count");
     for(std::uint64_t i=0;i<r.run_count();++i)check(r.validate_sorted(i,b),"initial sorted");}
    auto p=BuildBalancedPlan(dir/"initial",s);s={};
    ExecuteMergePlan(p,dir/"sorted",dir/"scratch",b,s);
    const auto expectedBytes=p?(p->type==MergePlanNode::Type::Leaf?4*n:4*p->estimatedCost):0;
    check(s.data_read_bytes==expectedBytes&&s.data_write_bytes==expectedBytes,"merge cost bytes");
    check(!std::filesystem::exists(dir/"scratch"),"scratch cleanup");
    RunFileReader result(dir/"sorted",s);check(result.run_count()==1&&result.directory(0).count==n,"final count");
    std::sort(oracle.begin(),oracle.end());std::size_t i=0;
    result.scan(0,0,n,b,[&](Value v){check(v==oracle[i++],"full multiset oracle");});
    rejects([&]{ExecuteMergePlan(p,dir/"sorted",dir/"other",b,s);});
 }
 // Unequal and empty inputs: exact two-way merge block formula.
 const auto initial=box.path/"unequal";
 {RunFileWriter w(initial,3,2,s);w.begin_run();w.end_run();w.begin_run();for(Value v:{-9,0,0,7,99})w.put(v);w.end_run();w.begin_run();for(Value v:{-10,1})w.put(v);w.end_run();w.finish();}
 s={};PerformTwoWayMerge({initial,1,5},{initial,2,2},box.path/"merged",2,s);
 check(s.data_reads==4&&s.data_writes==4&&s.data_read_bytes==28&&s.data_write_bytes==28,"merge exact blocks");
 PerformTwoWayMerge({initial,0,0},{initial,1,5},box.path/"empty-merge",1,s);
 rejects([&]{PerformTwoWayMerge({initial,1,6},{initial,2,2},box.path/"bad",2,s);});
 rejects([&]{RunGeneration(box.path/"missing",box.path/"out",0,2,s);});
 {std::ofstream f(box.path/"partial",std::ios::binary);f.put('x');}
 rejects([&]{RunGeneration(box.path/"partial",box.path/"out",2,2,s);});
 auto badPlan=BuildBalancedPlan(initial,s);badPlan->sequenceLength++;
 rejects([&]{ExecuteMergePlan(badPlan,box.path/"bad-final",box.path/"failed-scratch",2,s);});
 check(!std::filesystem::exists(box.path/"bad-final")&&!std::filesystem::exists(box.path/"failed-scratch"),"failure cleanup");
 // Large file uses a counting oracle, no N-sized vector; repeats each key exactly 1000 times.
 constexpr std::uint64_t n=100000;
 generate_raw(box.path/"large-raw",n,[](std::uint64_t i){return static_cast<Value>(99-i%100);},31,s);
 RunGeneration(box.path/"large-raw",box.path/"large-initial",997,31,s);
 auto p=BuildBalancedPlan(box.path/"large-initial",s);
 ExecuteMergePlan(p,box.path/"large-result",box.path/"large-scratch",31,s);
 RunFileReader large(box.path/"large-result",s);std::uint64_t index=0;
 large.scan(0,0,n,31,[&](Value v){check(v==static_cast<Value>(index/1000),"large streaming oracle");++index;});
 std::cout<<"PASS: "<<cases<<" oracle cases; exact I/O; invalid input; cleanup; 100000-record bounded-memory sort\n";
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
