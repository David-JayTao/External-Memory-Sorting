#pragma once
#include "external/buffer.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

namespace external {
inline constexpr std::uint64_t header_bytes = 64, directory_entry_bytes = 32;
struct RunEntry {
    std::uint64_t id, count, offset, bytes;
};
// Construction protocol: begin_run -> put* -> end_run; repeat; finish.
// Existing paths are rejected. Only finish writes a valid magic signature.
class RunFileWriter {
  public:
    RunFileWriter(const std::filesystem::path &path, std::uint64_t max_runs,
                  std::size_t buffer_capacity, IOStats &stats);
    void begin_run();
    void put(Value value);
    void end_run();
    void finish();

  private:
    void ensure_open() const;
    std::fstream file_;
    IOStats &stats_;
    std::uint64_t max_runs_, runs_ = 0, data_start_, end_, count_ = 0;
    std::size_t capacity_;
    bool finished_ = false, failed_ = false;
    std::optional<Value> last_;
    std::unique_ptr<OutputBuffer> output_;
};
class RunFileReader {
  public:
    RunFileReader(const std::filesystem::path &path, IOStats &stats);
    std::uint64_t run_count() const {
        return runs_;
    }
    std::uint64_t max_runs() const {
        return max_runs_;
    }
    std::uint64_t data_start() const {
        return data_start_;
    }
    std::uint64_t file_end() const {
        return end_;
    }
    RunEntry directory(std::uint64_t run);
    InputBuffer input(std::uint64_t run, std::size_t capacity);
    void scan(std::uint64_t run, std::uint64_t first, std::uint64_t count, std::size_t capacity,
              const std::function<void(Value)> &visitor);
    std::optional<std::uint64_t> search(std::uint64_t run, Value key, std::size_t capacity);
    bool validate_sorted(std::uint64_t run, std::size_t capacity);
    void display(std::ostream &out, std::size_t capacity);

  private:
    std::ifstream file_;
    IOStats &stats_;
    std::uint64_t max_runs_ = 0, runs_ = 0, data_start_ = 0, end_ = 0;
};
} // namespace external
