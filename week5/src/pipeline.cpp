#include "external/advanced.hpp"
#include "external/bounded_queue.hpp"
#include "external/replacement_core.hpp"
#include <array>
#include <exception>
#include <thread>

namespace external {
namespace {
struct OutputChunk {
    std::vector<Value> values;
    bool end_run = false;
};
void add_stats(IOStats &to, const IOStats &from) {
    to.data_reads += from.data_reads;
    to.data_writes += from.data_writes;
    to.data_read_bytes += from.data_read_bytes;
    to.data_write_bytes += from.data_write_bytes;
    to.metadata_reads += from.metadata_reads;
    to.metadata_writes += from.metadata_writes;
    to.metadata_read_bytes += from.metadata_read_bytes;
    to.metadata_write_bytes += from.metadata_write_bytes;
}
} // namespace
PipelineMetrics PipelineReplacementSelection(const std::filesystem::path &input,
                                             const std::filesystem::path &output,
                                             std::size_t memory, std::size_t buffer, IOStats &stats,
                                             PipelineOptions options) {
    detail::checked_capacity(memory);
    detail::checked_capacity(buffer);
    if (!options.queue_blocks || options.queue_blocks > 1024)
        throw std::invalid_argument("queue capacity must be 1..1024 blocks");
    if (std::filesystem::exists(output))
        throw std::invalid_argument("pipeline output exists");
    const auto n = raw_count(input);
    BoundedQueue<std::vector<Value>> incoming(options.queue_blocks);
    BoundedQueue<OutputChunk> outgoing(options.queue_blocks);
    IOStats reader_stats, writer_stats; // thread-confined until all joins
    std::mutex error_mutex;
    std::exception_ptr error;
    auto fail = [&](std::exception_ptr e) {
        {
            std::lock_guard<std::mutex> lock(error_mutex);
            if (!error)
                error = e;
        }
        incoming.cancel();
        outgoing.cancel();
    };
    auto probe = [&](PipelineStage stage, std::uint64_t index) {
        if (options.fail_stage == stage && index == options.fail_after)
            throw std::runtime_error("injected pipeline stage failure");
    };
    auto read = [&] {
        std::ifstream file(input, std::ios::binary);
        if (!file)
            throw std::runtime_error("pipeline input open failed");
        InputBuffer source(file, 0, n, buffer, reader_stats);
        std::uint64_t blocks = 0;
        Value value = 0;
        while (true) {
            std::vector<Value> block;
            block.reserve(buffer);
            while (block.size() < buffer && source.next(value))
                block.push_back(value);
            if (block.empty())
                break;
            probe(PipelineStage::Read, blocks++);
            incoming.push(std::move(block));
        }
        incoming.close();
    };
    auto compute = [&] {
        std::vector<Value> input_block;
        std::size_t position = 0;
        OutputChunk output_block;
        output_block.values.reserve(buffer);
        std::uint64_t records = 0;
        auto next = [&](Value &value) {
            if (position == input_block.size()) {
                if (!incoming.pop(input_block))
                    return false;
                position = 0;
            }
            value = input_block[position++];
            return true;
        };
        auto flush = [&](bool end_run) {
            output_block.end_run = end_run;
            outgoing.push(std::move(output_block));
            output_block = OutputChunk{};
            output_block.values.reserve(buffer);
        };
        detail::replacement_core(
            memory, next, [] {},
            [&](Value value) {
                probe(PipelineStage::Compute, records++);
                output_block.values.push_back(value);
                if (output_block.values.size() == buffer)
                    flush(false);
            },
            [&] { flush(true); });
        outgoing.close();
    };
    auto write = [&] {
        RunFileWriter writer(output, n / memory + (n % memory != 0), buffer, writer_stats);
        OutputChunk chunk;
        bool active = false;
        std::uint64_t blocks = 0, count = 0;
        while (outgoing.pop(chunk)) {
            probe(PipelineStage::Write, blocks++);
            if (!chunk.values.empty() && !active) {
                writer.begin_run();
                active = true;
            }
            for (auto value : chunk.values) {
                writer.put(value);
                ++count;
            }
            if (chunk.end_run) {
                if (!active)
                    throw std::logic_error("run end without values");
                writer.end_run();
                active = false;
            }
        }
        if (active || count != n)
            throw std::runtime_error("pipeline output count/end mismatch");
        writer.finish();
    };
    std::array<std::thread, 3> workers;
    try {
        workers[0] = std::thread([&] {
            try {
                read();
            } catch (...) {
                fail(std::current_exception());
            }
        });
        workers[1] = std::thread([&] {
            try {
                compute();
            } catch (...) {
                fail(std::current_exception());
            }
        });
        workers[2] = std::thread([&] {
            try {
                write();
            } catch (...) {
                fail(std::current_exception());
            }
        });
    } catch (...) {
        fail(std::current_exception());
    } // includes partial thread creation failure
    for (auto &worker : workers)
        if (worker.joinable())
            worker.join();
    add_stats(stats, reader_stats);
    add_stats(stats, writer_stats);
    if (error) {
        std::error_code ec;
        std::filesystem::remove(output, ec);
        std::rethrow_exception(error);
    }
    return {incoming.high_water(), outgoing.high_water()};
}
} // namespace external
