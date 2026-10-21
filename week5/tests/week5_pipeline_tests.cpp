#include "external/advanced.hpp"
#include "external/bounded_queue.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
using namespace external;
namespace {
void check(bool ok, const char *msg) {
    if (!ok)
        throw std::runtime_error(msg);
}
template <class F> void rejects(F f) {
    bool bad = false;
    try {
        f();
    } catch (const std::exception &) {
        bad = true;
    }
    check(bad, "expected failure");
}
struct Sandbox {
    std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        ("week5-pipeline-" +
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
bool equal_files(const std::filesystem::path &a, const std::filesystem::path &b) {
    if (std::filesystem::file_size(a) != std::filesystem::file_size(b))
        return false;
    std::ifstream x(a, std::ios::binary), y(b, std::ios::binary);
    char xb[4096], yb[4096];
    do {
        x.read(xb, sizeof xb);
        y.read(yb, sizeof yb);
        if (x.gcount() != y.gcount() || !std::equal(xb, xb + x.gcount(), yb))
            return false;
    } while (x.gcount());
    return true;
}
} // namespace
int main() {
    try {
        Sandbox box;
        int cases = 0;
        for (std::size_t n : {0U, 1U, 2U, 31U, 32U, 33U, 1003U})
            for (std::size_t b : {1U, 7U, 32U}) {
                const auto dir = box.p / std::to_string(cases++);
                std::filesystem::create_directory(dir);
                IOStats setup, serial, parallel;
                generate_raw(
                    dir / "raw", n,
                    [](std::uint64_t i) { return static_cast<Value>((i * 37) % 97) - 48; }, b,
                    setup);
                ReplacementSelection(dir / "raw", dir / "serial", 16, b, serial);
                auto metrics =
                    PipelineReplacementSelection(dir / "raw", dir / "parallel", 16, b, parallel);
                check(equal_files(dir / "serial", dir / "parallel"),
                      "serial/pipeline byte identity");
                check(serial.data_reads == parallel.data_reads &&
                          serial.data_writes == parallel.data_writes &&
                          serial.data_read_bytes == parallel.data_read_bytes &&
                          serial.data_write_bytes == parallel.data_write_bytes,
                      "serial/pipeline I/O identity");
                check(metrics.input_high_water <= 1 && metrics.output_high_water <= 1,
                      "bounded queue capacity");
            }
        IOStats stats;
        generate_raw(
            box.p / "raw", 10003, [](std::uint64_t i) { return -static_cast<Value>(i); }, 7, stats);
        for (auto stage : {PipelineStage::Read, PipelineStage::Compute, PipelineStage::Write})
            for (std::uint64_t after : {0ULL, 2ULL, 20ULL}) {
                const auto path = box.p / ("fault-" + std::to_string(cases++));
                bool caught = false;
                try {
                    PipelineReplacementSelection(box.p / "raw", path, 17, 7, stats,
                                                 {1, stage, after});
                } catch (const std::exception &e) {
                    caught = true;
                    check(std::string(e.what()) == "injected pipeline stage failure",
                          "original exception preserved");
                }
                check(caught, "injected failure must reach caller");
                check(!std::filesystem::exists(path), "failed pipeline output cleanup");
            }
        // Actual output-open failure in the writer thread must also cancel blocked peers.
        rejects([&] {
            PipelineReplacementSelection(box.p / "raw", box.p / "missing-parent" / "result", 17, 7,
                                         stats);
        });
        rejects([&] { PipelineReplacementSelection(box.p / "raw", box.p / "raw", 17, 7, stats); });
        rejects([&] {
            PipelineReplacementSelection(box.p / "raw", box.p / "invalid", 17, 7, stats, {0});
        });
        for (int repeat = 0; repeat < 3; ++repeat) {
            const auto path = box.p / ("repeat" + std::to_string(repeat));
            auto metrics = PipelineReplacementSelection(box.p / "raw", path, 17, 7, stats, {2});
            check(metrics.input_high_water <= 2 && metrics.output_high_water <= 2,
                  "queue capacity two");
            RunFileReader r(path, stats);
            check(r.run_count() == 589, "descending pipeline run count");
        }
        // Direct queue lifecycle: close drains values, cancellation wakes a blocked
        // producer/consumer.
        {
            BoundedQueue<int> q(1);
            q.push(4);
            q.close();
            int v = 0;
            check(q.pop(v) && v == 4 && !q.pop(v), "queue drain");
            rejects([&] { q.push(5); });
        }
        {
            BoundedQueue<int> q(1);
            q.push(1);
            bool caught = false;
            std::thread t([&] {
                try {
                    q.push(2);
                } catch (...) {
                    caught = true;
                }
            });
            q.cancel();
            t.join();
            check(caught, "producer cancellation");
        }
        {
            BoundedQueue<int> q(1);
            bool caught = false;
            std::thread t([&] {
                try {
                    int v;
                    q.pop(v);
                } catch (...) {
                    caught = true;
                }
            });
            q.cancel();
            t.join();
            check(caught, "consumer cancellation");
        }
        // Large pipeline and final external merge: streaming analytic oracle, no whole-file vector.
        constexpr std::uint64_t n = 100000;
        generate_raw(
            box.p / "large", n, [](std::uint64_t i) { return static_cast<Value>(99 - i % 100); },
            127, stats);
        PipelineReplacementSelection(box.p / "large", box.p / "large-runs", 997, 127, stats);
        auto plan = BuildOptimalPlan(box.p / "large-runs", stats);
        ExecuteMergePlan(plan, box.p / "final", box.p / "scratch", 127, stats);
        RunFileReader result(box.p / "final", stats);
        std::uint64_t index = 0;
        result.scan(0, 0, n, 127, [&](Value v) {
            check(v == static_cast<Value>(index / 1000), "large pipeline sorted oracle");
            ++index;
        });
        std::cout << "PASS pipeline: 21 byte-identical serial comparisons; 9 injected stage "
                     "failures; real write-open failure; bounded queues; repeated joins; 100000 "
                     "records through optimal merge\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
