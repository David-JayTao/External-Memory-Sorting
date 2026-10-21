#include "external/advanced.hpp"
#include "external/loser_tree.hpp"
#include <queue>
#include <thread>

namespace external {
int ThreadJoinExample() {
    int answer = 0;
    std::thread worker([&] { answer = 42; });
    worker.join();
    return answer;
}
Plan BuildOptimalPlan(const std::filesystem::path &initial, IOStats &stats) {
    struct Greater {
        bool operator()(const Plan &a, const Plan &b) const {
            if (a->sequenceLength != b->sequenceLength)
                return a->sequenceLength > b->sequenceLength;
            return a->id > b->id;
        }
    };
    auto leaves = plan_leaves(initial, stats);
    std::uint64_t id = leaves.size();
    std::priority_queue<Plan, std::vector<Plan>, Greater> queue(Greater{}, std::move(leaves));
    while (queue.size() > 1) {
        auto a = queue.top();
        queue.pop();
        auto b = queue.top();
        queue.pop();
        queue.push(join_plans(a, b, id++));
    }
    return queue.empty() ? nullptr : queue.top();
}
} // namespace external
