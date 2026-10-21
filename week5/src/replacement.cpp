#include "external/advanced.hpp"
#include "external/replacement_core.hpp"
namespace external {
void ReplacementSelection(const std::filesystem::path &input, const std::filesystem::path &output,
                          std::size_t memory, std::size_t buffer, IOStats &stats) {
    detail::checked_capacity(memory);
    detail::checked_capacity(buffer);
    const auto count = raw_count(input);
    std::ifstream file(input, std::ios::binary);
    if (!file)
        throw std::runtime_error("cannot open replacement input");
    InputBuffer in(file, 0, count, buffer, stats);
    RunFileWriter writer(output, count / memory + (count % memory != 0), buffer, stats);
    detail::replacement_core(
        memory, [&](Value &v) { return in.next(v); }, [&] { writer.begin_run(); },
        [&](Value v) { writer.put(v); }, [&] { writer.end_run(); });
    writer.finish();
}
} // namespace external
