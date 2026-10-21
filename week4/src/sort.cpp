#include "external/sort.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace external {
namespace {
std::uint64_t checked_add(std::uint64_t a, std::uint64_t b) {
    if (b > std::numeric_limits<std::uint64_t>::max() - a)
        throw std::overflow_error("merge size/cost overflow");
    return a + b;
}
void verify_ref(RunFileReader &reader, const RunRef &ref) {
    if (reader.directory(ref.run).count != ref.length)
        throw std::runtime_error("plan leaf length mismatch");
}
RunRef copy_run(const RunRef &ref, const std::filesystem::path &output, std::size_t buffer,
                IOStats &stats) {
    RunFileReader reader(ref.path, stats);
    verify_ref(reader, ref);
    auto in = reader.input(ref.run, buffer);
    RunFileWriter writer(output, 1, buffer, stats);
    writer.begin_run();
    Value value = 0;
    while (in.next(value))
        writer.put(value);
    writer.end_run();
    writer.finish();
    return {output, 0, ref.length};
}
// Postorder execution: child buffers have been destroyed before the parent allocates its three
// blocks.
RunRef execute(const Plan &node, const std::filesystem::path &output,
               const std::filesystem::path &scratch, std::size_t buffer, IOStats &stats) {
    if (node->status != MergeStatus::Pending)
        throw std::logic_error("plan is single-use");
    node->status = MergeStatus::Running;
    const auto before = stats.data_reads + stats.data_writes;
    try {
        if (node->type == MergePlanNode::Type::Leaf) {
            RunFileReader reader(node->source.path, stats);
            verify_ref(reader, node->source);
            node->status = MergeStatus::Complete;
            return node->source;
        }
        if (!node->leftChild || !node->rightChild)
            throw std::invalid_argument("internal node requires two children");
        const auto left =
            execute(node->leftChild, scratch / (std::to_string(node->leftChild->id) + ".runs"),
                    scratch, buffer, stats);
        const auto right =
            execute(node->rightChild, scratch / (std::to_string(node->rightChild->id) + ".runs"),
                    scratch, buffer, stats);
        if (checked_add(left.length, right.length) != node->sequenceLength)
            throw std::runtime_error("plan total mismatch");
        const auto merge_before = stats.data_reads + stats.data_writes;
        auto result = PerformTwoWayMerge(left, right, output, buffer, stats);
        node->ioOperations = stats.data_reads + stats.data_writes - merge_before;
        node->outputPath = output;
        node->status = MergeStatus::Complete;
        // Delete only intermediates produced in our scratch directory, never leaf sources.
        if (node->leftChild->type == MergePlanNode::Type::Internal)
            std::filesystem::remove(left.path);
        if (node->rightChild->type == MergePlanNode::Type::Internal)
            std::filesystem::remove(right.path);
        return result;
    } catch (...) {
        node->ioOperations = stats.data_reads + stats.data_writes - before;
        node->status = MergeStatus::Failed;
        throw;
    }
}
} // namespace
std::uint64_t raw_count(const std::filesystem::path &path) {
    const auto bytes = std::filesystem::file_size(path);
    if (bytes % 4)
        throw std::runtime_error("raw input has incomplete int32 record");
    return bytes / 4;
}
void generate_raw(const std::filesystem::path &path, std::uint64_t count,
                  const std::function<Value(std::uint64_t)> &source, std::size_t buffer,
                  IOStats &stats) {
    detail::checked_capacity(buffer);
    if (std::filesystem::exists(path))
        throw std::invalid_argument("raw output exists");
    std::ofstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("cannot create raw file");
    OutputBuffer out(file, 0, buffer, stats);
    for (std::uint64_t i = 0; i < count; ++i)
        out.put(source(i));
    out.flush();
    file.close();
    if (!file)
        throw std::runtime_error("raw output close failed");
}
void RunGeneration(const std::filesystem::path &input, const std::filesystem::path &output,
                   std::size_t memory_records, std::size_t buffer, IOStats &stats) {
    detail::checked_capacity(memory_records);
    detail::checked_capacity(buffer);
    const auto n = raw_count(input), runs = n / memory_records + (n % memory_records != 0);
    std::ifstream file(input, std::ios::binary);
    if (!file)
        throw std::runtime_error("cannot open raw file");
    InputBuffer in(file, 0, n, buffer, stats);
    RunFileWriter writer(output, runs, buffer, stats);
    std::vector<Value> chunk;
    chunk.reserve(memory_records);
    Value value = 0;
    std::uint64_t consumed = 0;
    while (consumed < n) {
        chunk.clear();
        while (chunk.size() < memory_records && in.next(value)) {
            chunk.push_back(value);
            ++consumed;
        }
        std::sort(chunk.begin(), chunk.end());
        writer.begin_run();
        for (auto v : chunk)
            writer.put(v);
        writer.end_run();
    }
    writer.finish();
}
std::vector<Plan> plan_leaves(const std::filesystem::path &initial, IOStats &stats) {
    RunFileReader reader(initial, stats);
    std::vector<Plan> nodes;
    for (std::uint64_t i = 0; i < reader.run_count(); ++i) {
        auto node = std::make_shared<MergePlanNode>();
        node->id = i;
        node->sequenceLength = reader.directory(i).count;
        node->source = {initial, i, node->sequenceLength};
        nodes.push_back(node);
    }
    return nodes;
}
Plan join_plans(Plan left, Plan right, std::uint64_t id) {
    auto parent = std::make_shared<MergePlanNode>();
    parent->type = MergePlanNode::Type::Internal;
    parent->id = id;
    parent->leftChild = std::move(left);
    parent->rightChild = std::move(right);
    parent->sequenceLength =
        checked_add(parent->leftChild->sequenceLength, parent->rightChild->sequenceLength);
    parent->estimatedCost = checked_add(
        checked_add(parent->leftChild->estimatedCost, parent->rightChild->estimatedCost),
        parent->sequenceLength);
    return parent;
}
Plan BuildBalancedPlan(const std::filesystem::path &initial, IOStats &stats) {
    auto nodes = plan_leaves(initial, stats);
    std::uint64_t id = nodes.size();
    while (nodes.size() > 1) {
        std::vector<Plan> next;
        for (std::size_t i = 0; i < nodes.size(); i += 2)
            next.push_back(i + 1 < nodes.size() ? join_plans(nodes[i], nodes[i + 1], id++)
                                                : nodes[i]);
        nodes = std::move(next);
    }
    return nodes.empty() ? nullptr : nodes.front();
}
RunRef PerformTwoWayMerge(const RunRef &left, const RunRef &right,
                          const std::filesystem::path &output, std::size_t buffer, IOStats &stats) {
    const auto expected = checked_add(left.length, right.length);
    RunFileReader a(left.path, stats), b(right.path, stats);
    verify_ref(a, left);
    verify_ref(b, right);
    auto x = a.input(left.run, buffer), y = b.input(right.run, buffer);
    RunFileWriter writer(output, 1, buffer, stats);
    writer.begin_run();
    Value xv = 0, yv = 0;
    bool has_x = x.next(xv), has_y = y.next(yv);
    std::uint64_t count = 0;
    while (has_x || has_y) {
        if (has_x && (!has_y || xv <= yv)) {
            writer.put(xv);
            has_x = x.next(xv);
        } else {
            writer.put(yv);
            has_y = y.next(yv);
        }
        ++count;
    }
    if (count != expected)
        throw std::runtime_error("merge count mismatch");
    writer.end_run();
    writer.finish();
    return {output, 0, count};
}
void ExecuteMergePlan(const Plan &root, const std::filesystem::path &output,
                      const std::filesystem::path &scratch, std::size_t buffer, IOStats &stats) {
    detail::checked_capacity(buffer);
    if (std::filesystem::exists(output))
        throw std::invalid_argument("final output exists");
    const auto relative = std::filesystem::absolute(output).lexically_normal().lexically_relative(
        std::filesystem::absolute(scratch).lexically_normal());
    if (!relative.empty() && *relative.begin() != "..")
        throw std::invalid_argument("output must be outside scratch");
    if (!std::filesystem::create_directory(scratch))
        throw std::invalid_argument("scratch must be new");
    // This guard owns only the directory it just successfully created.
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    } cleanup{scratch};
    try {
        if (!root) {
            RunFileWriter w(output, 1, buffer, stats);
            w.begin_run();
            w.end_run();
            w.finish();
            return;
        }
        auto result = execute(root, output, scratch, buffer, stats);
        if (root->type == MergePlanNode::Type::Leaf) {
            const auto before = stats.data_reads + stats.data_writes;
            copy_run(result, output, buffer, stats);
            root->outputPath = output;
            root->ioOperations = stats.data_reads + stats.data_writes - before;
        }
    } catch (...) {
        if (root)
            root->status = MergeStatus::Failed;
        std::error_code ec;
        std::filesystem::remove(output, ec);
        throw;
    }
}
} // namespace external
