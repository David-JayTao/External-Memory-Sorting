#include "external/sort.hpp"
#include <iostream>
using namespace external;
int main(int argc,char** argv) {
    try {
        const std::filesystem::path dir=argc>1?argv[1]:"week4-demo";
        if (!std::filesystem::create_directory(dir)) throw std::runtime_error("demo directory must be new");
        IOStats stats;
        generate_raw(dir/"input.raw",23,[](std::uint64_t i){return static_cast<Value>((i*17)%31)-15;},4,stats);
        RunGeneration(dir/"input.raw",dir/"initial.runs",5,4,stats);
        auto plan=BuildBalancedPlan(dir/"initial.runs",stats);
        std::cout<<"Initial runs=5, merge cost="<<plan->estimatedCost<<'\n';
        ExecuteMergePlan(plan,dir/"sorted.runs",dir/"scratch",4,stats);
        RunFileReader reader(dir/"sorted.runs",stats); reader.display(std::cout,4);
        std::cout<<"Data reads="<<stats.data_reads<<" writes="<<stats.data_writes<<'\n';
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
