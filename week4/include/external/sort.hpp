#pragma once
#include "run_file.hpp"
#include <string>

namespace external {
// Raw input is a sequence of little-endian int32 records, without a header.
std::uint64_t raw_count(const std::filesystem::path &path);
void generate_raw(const std::filesystem::path &path, std::uint64_t count,
                  const std::function<Value(std::uint64_t)> &source, std::size_t buffer,
                  IOStats &stats);
void RunGeneration(const std::filesystem::path &input, const std::filesystem::path &output,
                   std::size_t memory_records, std::size_t buffer, IOStats &stats);
struct RunRef {
    std::filesystem::path path;
    std::uint64_t run = 0, length = 0;
};
enum class MergeStatus { Pending, Running, Complete, Failed };
struct MergePlanNode {
    enum class Type { Leaf, Internal };
    Type type = Type::Leaf;
    std::uint64_t id = 0, sequenceLength = 0, estimatedCost = 0, ioOperations = 0;
    RunRef source;
    std::filesystem::path outputPath;
    MergeStatus status = MergeStatus::Pending;
    std::shared_ptr<MergePlanNode> leftChild, rightChild;
};
using Plan = std::shared_ptr<MergePlanNode>;
std::vector<Plan> plan_leaves(const std::filesystem::path &initial, IOStats &stats);
Plan join_plans(Plan left, Plan right, std::uint64_t id);
Plan BuildBalancedPlan(const std::filesystem::path &initial, IOStats &stats);
RunRef PerformTwoWayMerge(const RunRef &left, const RunRef &right,
                          const std::filesystem::path &output, std::size_t buffer, IOStats &stats);
// Output and scratch must not exist. Scratch is exclusively owned and removed on exit.
void ExecuteMergePlan(const Plan &root, const std::filesystem::path &output,
                      const std::filesystem::path &scratch, std::size_t buffer, IOStats &stats);
} // namespace external
