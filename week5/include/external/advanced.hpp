#pragma once
#include "sort.hpp"
namespace external {
int ThreadJoinExample();
Plan BuildOptimalPlan(const std::filesystem::path &initial, IOStats &stats);
void ReplacementSelection(const std::filesystem::path &input, const std::filesystem::path &output,
                          std::size_t memory_records, std::size_t buffer, IOStats &stats);
enum class PipelineStage { None, Read, Compute, Write };
struct PipelineOptions {
    std::size_t queue_blocks = 1;
    // Deterministic fault injection for tests: block index for Read/Write, record index for
    // Compute.
    PipelineStage fail_stage = PipelineStage::None;
    std::uint64_t fail_after = 0;
};
struct PipelineMetrics {
    std::size_t input_high_water = 0, output_high_water = 0;
};
PipelineMetrics PipelineReplacementSelection(const std::filesystem::path &input,
                                             const std::filesystem::path &output,
                                             std::size_t memory_records, std::size_t buffer,
                                             IOStats &stats, PipelineOptions options = {});
} // namespace external
