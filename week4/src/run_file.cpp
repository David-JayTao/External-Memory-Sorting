#include "external/run_file.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace external {
namespace {
constexpr char magic[8] = {'R', 'U', 'N', 'F', '1', '7', '\r', '\n'};
// Reserve at most 32 MiB of directory space; entries are never all held in RAM.
constexpr std::uint64_t max_directory_entries = 1024 * 1024;
void meta_read(std::istream &f, IOStats &s, std::uint64_t pos, char *p, std::size_t n) {
    detail::read_exact(f, pos, p, n);
    ++s.metadata_reads;
    s.metadata_read_bytes += n;
}
void meta_write(std::ostream &f, IOStats &s, std::uint64_t pos, const char *p, std::size_t n) {
    detail::write_exact(f, pos, p, n);
    ++s.metadata_writes;
    s.metadata_write_bytes += n;
}
} // namespace
RunFileWriter::RunFileWriter(const std::filesystem::path &path, std::uint64_t max_runs,
                             std::size_t capacity, IOStats &stats)
    : stats_(stats), max_runs_(max_runs), data_start_(0), end_(0),
      capacity_(detail::checked_capacity(capacity)) {
    if (max_runs > max_directory_entries)
        throw std::invalid_argument("too many directory slots");
    if (std::filesystem::exists(path))
        throw std::invalid_argument("output path already exists");
    data_start_ = header_bytes + max_runs * directory_entry_bytes;
    end_ = data_start_;
    file_.open(path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!file_)
        throw std::runtime_error("cannot create output file");
    // Fixed-size initialization chunks, including an invalid provisional header.
    std::array<char, 4096> zero{};
    for (std::uint64_t pos = 0; pos < data_start_;) {
        const auto n =
            static_cast<std::size_t>(std::min<std::uint64_t>(zero.size(), data_start_ - pos));
        meta_write(file_, stats_, pos, zero.data(), n);
        pos += n;
    }
}
void RunFileWriter::ensure_open() const {
    if (finished_ || failed_)
        throw std::logic_error("writer is finished or failed");
}
void RunFileWriter::begin_run() {
    ensure_open();
    if (output_)
        throw std::logic_error("a run is already active");
    if (runs_ == max_runs_)
        throw std::length_error("directory is full");
    output_ = std::make_unique<OutputBuffer>(file_, end_, capacity_, stats_);
    count_ = 0;
    last_.reset();
}
void RunFileWriter::put(Value value) {
    ensure_open();
    if (!output_)
        throw std::logic_error("begin_run required");
    if (last_ && value < *last_)
        throw std::invalid_argument("Put would violate sorted run order");
    const auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max());
    if (count_ >= (limit - end_) / 4)
        throw std::overflow_error("run too long");
    try {
        output_->put(value);
    } catch (...) {
        failed_ = true;
        throw;
    }
    ++count_;
    last_ = value;
}
void RunFileWriter::end_run() {
    ensure_open();
    if (!output_)
        throw std::logic_error("no active run");
    try {
        output_->flush();
        std::array<char, 32> entry{};
        detail::encode(entry.data(), runs_, 8);
        detail::encode(entry.data() + 8, count_, 8);
        detail::encode(entry.data() + 16, end_, 8);
        detail::encode(entry.data() + 24, count_ * 4, 8);
        meta_write(file_, stats_, header_bytes + runs_ * directory_entry_bytes, entry.data(),
                   entry.size());
    } catch (...) {
        failed_ = true;
        throw;
    }
    end_ += count_ * 4;
    ++runs_;
    output_.reset();
}
void RunFileWriter::finish() {
    ensure_open();
    if (output_)
        throw std::logic_error("end_run required before finish");
    std::array<char, 64> h{};
    std::memcpy(h.data(), magic, 8);
    detail::encode(h.data() + 8, 1, 4);  // version
    detail::encode(h.data() + 12, 4, 4); // bytes per value
    detail::encode(h.data() + 16, max_runs_, 8);
    detail::encode(h.data() + 24, runs_, 8);
    detail::encode(h.data() + 32, header_bytes, 8);
    detail::encode(h.data() + 40, data_start_, 8);
    detail::encode(h.data() + 48, end_, 8); // exclusive end
    try {
        file_.flush();
        if (!file_)
            throw std::runtime_error("data flush failed");
        meta_write(file_, stats_, 0, h.data(), h.size());
        file_.flush();
        if (!file_)
            throw std::runtime_error("header flush failed");
        file_.close();
        if (file_.fail())
            throw std::runtime_error("close failed");
    } catch (...) {
        failed_ = true;
        throw;
    }
    finished_ = true;
}
RunFileReader::RunFileReader(const std::filesystem::path &path, IOStats &stats) : stats_(stats) {
    file_.open(path, std::ios::binary);
    if (!file_)
        throw std::runtime_error("cannot open input file");
    const auto size = std::filesystem::file_size(path);
    std::array<char, 64> h{};
    meta_read(file_, stats_, 0, h.data(), h.size());
    if (std::memcmp(h.data(), magic, 8) != 0 || detail::decode(h.data() + 8, 4) != 1 ||
        detail::decode(h.data() + 12, 4) != 4 || detail::decode(h.data() + 56, 8) != 0)
        throw std::runtime_error("invalid magic/version/value size/reserved field");
    max_runs_ = detail::decode(h.data() + 16, 8);
    runs_ = detail::decode(h.data() + 24, 8);
    data_start_ = detail::decode(h.data() + 40, 8);
    end_ = detail::decode(h.data() + 48, 8);
    if (max_runs_ > max_directory_entries || runs_ > max_runs_ ||
        detail::decode(h.data() + 32, 8) != header_bytes ||
        data_start_ != header_bytes + max_runs_ * directory_entry_bytes || end_ < data_start_ ||
        end_ != size ||
        end_ > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
        throw std::runtime_error("invalid header bounds or file size");
    // Validate directory incrementally: no O(number of runs) metadata allocation.
    std::uint64_t expected = data_start_;
    for (std::uint64_t i = 0; i < runs_; ++i) {
        const auto e = directory(i);
        if (e.offset != expected)
            throw std::runtime_error("directory gap or overlap");
        expected += e.bytes;
    }
    if (expected != end_)
        throw std::runtime_error("unreferenced data at end");
}
RunEntry RunFileReader::directory(std::uint64_t run) {
    if (run >= runs_)
        throw std::out_of_range("run id");
    std::array<char, 32> e{};
    meta_read(file_, stats_, header_bytes + run * directory_entry_bytes, e.data(), e.size());
    RunEntry result{detail::decode(e.data(), 8), detail::decode(e.data() + 8, 8),
                    detail::decode(e.data() + 16, 8), detail::decode(e.data() + 24, 8)};
    if (result.id != run || result.offset < data_start_ || result.offset > end_ ||
        result.count > (end_ - result.offset) / 4 || result.bytes != result.count * 4)
        throw std::runtime_error("invalid directory entry");
    return result;
}
InputBuffer RunFileReader::input(std::uint64_t run, std::size_t capacity) {
    const auto e = directory(run);
    return InputBuffer(file_, e.offset, e.count, capacity, stats_);
}
void RunFileReader::scan(std::uint64_t run, std::uint64_t first, std::uint64_t count,
                         std::size_t capacity, const std::function<void(Value)> &visitor) {
    const auto e = directory(run);
    if (first > e.count || count > e.count - first)
        throw std::out_of_range("scan range");
    InputBuffer buffer(file_, e.offset, e.count, capacity, stats_);
    buffer.seek(first);
    for (std::uint64_t i = 0; i < count; ++i) {
        Value v = 0;
        buffer.next(v);
        visitor(v);
    }
}
std::optional<std::uint64_t> RunFileReader::search(std::uint64_t run, Value key,
                                                   std::size_t capacity) {
    const auto e = directory(run);
    InputBuffer buffer(file_, e.offset, e.count, capacity, stats_);
    std::uint64_t low = 0, high = e.count;
    // lower_bound: first index whose value is >= key, including duplicates.
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        if (buffer.at(mid) < key)
            low = mid + 1;
        else
            high = mid;
    }
    if (low < e.count && buffer.at(low) == key)
        return low;
    return std::nullopt;
}
bool RunFileReader::validate_sorted(std::uint64_t run, std::size_t capacity) {
    auto buffer = input(run, capacity);
    Value value = 0;
    std::optional<Value> previous;
    while (buffer.next(value)) {
        if (previous && value < *previous)
            return false;
        previous = value;
    }
    return true;
}
void RunFileReader::display(std::ostream &out, std::size_t capacity) {
    out << "Header: runs=" << runs_ << " capacity=" << max_runs_ << " data=" << data_start_
        << " end=" << end_ << '\n';
    for (std::uint64_t i = 0; i < runs_; ++i) {
        const auto e = directory(i);
        out << "Run " << i << " count=" << e.count << " offset=" << e.offset << " bytes=" << e.bytes
            << ":";
        scan(i, 0, e.count, capacity, [&](Value v) { out << ' ' << v; });
        out << '\n';
    }
}
} // namespace external
