#include "external/buffer.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace external {
namespace detail {
void encode(char* p, std::uint64_t value, unsigned bytes) {
    for (unsigned i=0; i<bytes; ++i) p[i]=static_cast<char>((value>>(8*i))&255);
}
std::uint64_t decode(const char* p, unsigned bytes) {
    std::uint64_t value=0;
    for (unsigned i=0; i<bytes; ++i) value|=std::uint64_t(static_cast<unsigned char>(p[i]))<<(8*i);
    return value;
}
std::size_t checked_capacity(std::size_t capacity) {
    // A generous, explicit teaching implementation limit (64 MiB encoded block).
    if (!capacity || capacity>16*1024*1024) throw std::invalid_argument("buffer capacity must be 1..16777216 records");
    return capacity;
}
static void check_range(std::uint64_t offset, std::size_t bytes) {
    const auto limit=static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max());
    if (offset>limit || bytes>limit-offset || bytes>static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::overflow_error("file position overflow");
}
void read_exact(std::istream& in, std::uint64_t offset, char* p, std::size_t bytes) {
    check_range(offset,bytes);
    in.clear();
    in.seekg(static_cast<std::streamoff>(offset));
    if (!in) throw std::runtime_error("seekg failed");
    in.read(p,static_cast<std::streamsize>(bytes));
    if (!in) throw std::runtime_error("short read or read failure");
}
void write_exact(std::ostream& out, std::uint64_t offset, const char* p, std::size_t bytes) {
    check_range(offset,bytes);
    if (!out) throw std::runtime_error("output stream already failed");
    out.seekp(static_cast<std::streamoff>(offset));
    if (!out) throw std::runtime_error("seekp failed");
    out.write(p,static_cast<std::streamsize>(bytes));
    if (!out) throw std::runtime_error("write failed");
}
}
InputBuffer::InputBuffer(std::istream& in, std::uint64_t offset, std::uint64_t count,
                         std::size_t capacity, IOStats& stats)
    : in_(in),stats_(stats),offset_(offset),count_(count),
      capacity_(detail::checked_capacity(capacity)),block_(capacity_*4) {
    const auto limit=static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max());
    if (offset>limit || count>(limit-offset)/4) throw std::overflow_error("run address overflow");
}
Value InputBuffer::at(std::uint64_t index) {
    if (index>=count_) throw std::out_of_range("record index");
    if (!valid_ || index<block_start_ || index-block_start_>=valid_) {
        const auto start=(index/capacity_)*capacity_;
        const auto n=static_cast<std::size_t>(std::min<std::uint64_t>(capacity_,count_-start));
        valid_=0; // A failed read must never leave a seemingly valid cache block.
        detail::read_exact(in_,offset_+start*4,block_.data(),n*4);
        block_start_=start; valid_=n;
        ++stats_.data_reads; stats_.data_read_bytes+=n*4;
    }
    const auto u=detail::decode(block_.data()+(index-block_start_)*4,4);
    // Explicit two's-complement decoding, no implementation-defined unsigned cast.
    const auto signed_value=u<=2147483647ULL ? static_cast<std::int64_t>(u)
                                            : static_cast<std::int64_t>(u)-4294967296LL;
    return static_cast<Value>(signed_value);
}
bool InputBuffer::next(Value& value) {
    if (cursor_==count_) return false;
    value=at(cursor_); ++cursor_; return true;
}
void InputBuffer::seek(std::uint64_t index) {
    if (index>count_) throw std::out_of_range("cursor index");
    cursor_=index;
}
OutputBuffer::OutputBuffer(std::ostream& out, std::uint64_t offset, std::size_t capacity, IOStats& stats)
    :out_(out),stats_(stats),offset_(offset),capacity_(detail::checked_capacity(capacity)),block_(capacity_*4) {}
void OutputBuffer::put(Value value) {
    if (failed_) throw std::logic_error("output buffer failed");
    detail::encode(block_.data()+used_*4,static_cast<std::uint32_t>(value),4);
    ++used_;
    if (used_==capacity_) flush();
}
void OutputBuffer::flush() {
    if (failed_) throw std::logic_error("output buffer failed");
    if (!used_) return;
    try { detail::write_exact(out_,offset_,block_.data(),used_*4); }
    catch (...) { failed_=true; throw; }
    ++stats_.data_writes; stats_.data_write_bytes+=used_*4;
    offset_+=used_*4; used_=0;
}
}
