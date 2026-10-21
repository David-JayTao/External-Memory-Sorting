#include "external/advanced.hpp"
#include <iostream>
using namespace external;
int main(int argc, char **argv) {
    try {
        const std::filesystem::path dir = argc > 1 ? argv[1] : "week5-demo";
        if (!std::filesystem::create_directory(dir))
            throw std::runtime_error("demo directory must be new");
        IOStats stats;
        std::cout << "thread + join result=" << ThreadJoinExample() << '\n';
        generate_raw(
            dir / "input.raw", 1003,
            [](std::uint64_t i) { return static_cast<Value>((i * 7919) % 1009) - 504; }, 16, stats);
        ReplacementSelection(dir / "input.raw", dir / "serial.runs", 31, 16, stats);
        auto metrics =
            PipelineReplacementSelection(dir / "input.raw", dir / "pipeline.runs", 31, 16, stats);
        auto optimal = BuildOptimalPlan(dir / "pipeline.runs", stats);
        auto balanced = BuildBalancedPlan(dir / "pipeline.runs", stats);
        RunFileReader runs(dir / "pipeline.runs", stats);
        std::cout << "Replacement runs=" << runs.run_count()
                  << "; balanced cost=" << balanced->estimatedCost
                  << "; optimal cost=" << optimal->estimatedCost
                  << "\nQueue high water: input=" << metrics.input_high_water
                  << " output=" << metrics.output_high_water << '\n';
        ExecuteMergePlan(optimal, dir / "sorted.runs", dir / "scratch", 16, stats);
        RunFileReader sorted(dir / "sorted.runs", stats);
        std::cout << "Final count=" << sorted.directory(0).count
                  << " sorted=" << sorted.validate_sorted(0, 16) << '\n';
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
