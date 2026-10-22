#include "external/advanced.hpp"
#include "external/loser_tree.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <random>
using namespace external;
namespace {
void check(bool ok, const char *msg) {
    if (!ok)
        throw std::runtime_error(msg);
}
std::uint64_t brute(std::vector<std::uint64_t> weights) {
    if (weights.size() < 2)
        return 0;
    auto best = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t i = 0; i < weights.size(); ++i)
        for (std::size_t j = i + 1; j < weights.size(); ++j) {
            std::vector<std::uint64_t> next;
            for (std::size_t k = 0; k < weights.size(); ++k)
                if (k != i && k != j)
                    next.push_back(weights[k]);
            const auto sum = weights[i] + weights[j];
            next.push_back(sum);
            best = std::min(best, sum + brute(next));
        }
    return best;
}
struct Sandbox {
    std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        ("week5-serial-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Sandbox() {
        if (!std::filesystem::create_directory(p))
            throw std::runtime_error("collision");
    }
    ~Sandbox() {
        std::error_code ec;
        std::filesystem::remove_all(p, ec);
    }
};
} // namespace
int main() {
    try {
        check(ThreadJoinExample() == 42, "thread join visibility");
        std::mt19937 rng(123);
        std::uint64_t comparisons = 0;
        for (std::size_t k : {0U, 1U, 2U, 3U, 7U, 16U, 31U}) {
            std::vector<std::optional<int>> oracle(k);
            for (auto &v : oracle)
                v = static_cast<int>(rng() % 99) - 49;
            LoserTree<int> tree(oracle);
            for (int step = 0; step < 500; ++step) {
                std::optional<int> best;
                std::size_t idx = 0;
                for (std::size_t i = 0; i < k; ++i)
                    if (oracle[i] && (!best || *oracle[i] < *best)) {
                        best = oracle[i];
                        idx = i;
                    }
                check(tree.empty() == !best, "tree empty oracle");
                if (!best)
                    break;
                check(tree.top() == *best && tree.winner_index() == idx, "loser tournament oracle");
                ++comparisons;
                const auto replacement =
                    step < 400 ? std::optional<int>(static_cast<int>(rng() % 101) - 50)
                               : std::nullopt;
                oracle[idx] = replacement;
                tree.replace_winner(replacement);
            }
        }
        Sandbox box;
        IOStats stats;
        int cases = 0;
        for (std::size_t m : {1U, 3U, 16U})
            for (int pattern = 0; pattern < 5; ++pattern) {
                auto dir = box.p / std::to_string(cases++);
                std::filesystem::create_directory(dir);
                std::vector<Value> input(pattern == 0 ? 0 : 103);
                for (std::size_t i = 0; i < input.size(); ++i)
                    input[i] = pattern == 1   ? static_cast<Value>(i)
                               : pattern == 2 ? -static_cast<Value>(i)
                               : pattern == 3 ? 7
                                              : static_cast<Value>(rng() % 101) - 50;
                if (pattern == 4) {
                    input[0] = std::numeric_limits<Value>::min();
                    input[1] = std::numeric_limits<Value>::max();
                }
                generate_raw(
                    dir / "raw", input.size(), [&](std::uint64_t i) { return input[i]; }, 5, stats);
                stats = {};
                ReplacementSelection(dir / "raw", dir / "runs", m, 5, stats);
                check(stats.data_read_bytes == input.size() * 4 &&
                          stats.data_write_bytes == input.size() * 4,
                      "replacement conservation");
                RunFileReader reader(dir / "runs", stats);
                std::vector<Value> actual;
                if (pattern == 1 || pattern == 3)
                    check(reader.run_count() == 1, "ascending/equal gives one long run");
                if (pattern == 2)
                    check(reader.run_count() == (input.size() + m - 1) / m,
                          "descending worst case");
                for (std::uint64_t r = 0; r < reader.run_count(); ++r) {
                    check(reader.validate_sorted(r, 5), "replacement run sorted");
                    reader.scan(r, 0, reader.directory(r).count, 5,
                                [&](Value v) { actual.push_back(v); });
                }
                std::sort(actual.begin(), actual.end());
                std::sort(input.begin(), input.end());
                check(actual == input, "replacement multiset");
                auto p = BuildOptimalPlan(dir / "runs", stats);
                ExecuteMergePlan(p, dir / "sorted", dir / "scratch", 5, stats);
                RunFileReader final(dir / "sorted", stats);
                std::size_t i = 0;
                final.scan(0, 0, input.size(), 5,
                           [&](Value v) { check(v == input[i++], "optimal final oracle"); });
            }
        for (const auto &weights : std::vector<std::vector<std::uint64_t>>{
                 {1, 2, 3, 100}, {0, 1, 2, 2}, {3, 3, 1}, {2, 4, 5, 7, 9}, {}, {9}}) {
            auto path = box.p / ("weights" + std::to_string(cases++));
            RunFileWriter w(path, weights.size(), 3, stats);
            for (auto n : weights) {
                w.begin_run();
                for (std::uint64_t i = 0; i < n; ++i)
                    w.put(0);
                w.end_run();
            }
            w.finish();
            auto optimal = BuildOptimalPlan(path, stats);
            check((optimal ? optimal->estimatedCost : 0) == brute(weights),
                  "optimal exhaustive oracle");
            if (weights == std::vector<std::uint64_t>{1, 2, 3, 100}) {
                check(optimal->estimatedCost == 115, "known Huffman cost");
                check(BuildBalancedPlan(path, stats)->estimatedCost == 212, "balanced comparison");
            }
        }
        std::cout << "PASS serial: " << comparisons
                  << " loser-tree replacements; 15 run-generation cases; optimal cost verified by "
                     "exhaustive pair choices; thread/join\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
