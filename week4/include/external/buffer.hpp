#pragma once
#include <cstdint>
#include <fstream>
#include <vector>

namespace external {
using Value = std::int32_t;
// Successful application-level transfers, NOT physical disk operations.
struct IOStats {
    std::uint64_t data_reads = 0, data_writes = 0, data_read_bytes = 0, data_write_bytes = 0;
    std::uint64_t metadata_reads = 0, metadata_writes = 0;
    std::uint64_t metadata_read_bytes = 0, metadata_write_bytes = 0;
};
namespace detail {
void encode(char *p, std::uint64_t value, unsigned bytes);
std::uint64_t decode(const char *p, unsigned bytes);
std::size_t checked_capacity(std::size_t capacity);
void read_exact(std::istream &in, std::uint64_t offset, char *p, std::size_t bytes);
void write_exact(std::ostream &out, std::uint64_t offset, const char *p, std::size_t bytes);
} // namespace detail
// Owns one encoded block; borrows a stream and stats which must outlive it.
class InputBuffer {
  public:
    InputBuffer(std::istream &in, std::uint64_t offset, std::uint64_t count, std::size_t capacity,
                IOStats &stats);
    bool next(Value &value);        // EOF leaves value unchanged; EOF does not issue I/O.
    Value at(std::uint64_t index);  // Does not change sequential cursor.
    void seek(std::uint64_t index); // count is a valid EOF cursor.
  private:
    std::istream &in_;
    IOStats &stats_;
    std::uint64_t offset_, count_, cursor_ = 0, block_start_ = 0;
    std::size_t capacity_, valid_ = 0;
    std::vector<char> block_;
};
// Explicit flush is required: destructors cannot reliably report I/O failures.
class OutputBuffer {
  public:
    OutputBuffer(std::ostream &out, std::uint64_t offset, std::size_t capacity, IOStats &stats);
    void put(Value value);
    void flush();

  private:
    std::ostream &out_;
    IOStats &stats_;
    std::uint64_t offset_;
    std::size_t capacity_, used_ = 0;
    bool failed_ = false;
    std::vector<char> block_;
};
} // namespace external
