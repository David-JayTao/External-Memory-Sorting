#pragma once
#include "buffer.hpp"
#include "loser_tree.hpp"

namespace external::detail {
struct SelectionKey {
    std::uint64_t generation;
    Value value;
};
struct SelectionLess {
    bool operator()(const SelectionKey &a, const SelectionKey &b) const {
        return a.generation != b.generation ? a.generation < b.generation : a.value < b.value;
    }
};
// Both serial and pipeline adapters use this exact state machine.
template <class Next, class Begin, class Put, class End>
void replacement_core(std::size_t memory, Next next, Begin begin, Put put, End end) {
    checked_capacity(memory);
    std::vector<std::optional<SelectionKey>> slots(memory);
    Value value = 0;
    for (std::size_t i = 0; i < memory && next(value); ++i)
        slots[i] = SelectionKey{0, value};
    LoserTree<SelectionKey, SelectionLess> tree(std::move(slots));
    std::uint64_t current = 0;
    bool active = false;
    while (!tree.empty()) {
        const auto winner = tree.top();
        if (!active || winner.generation != current) {
            if (active)
                end();
            begin();
            active = true;
            current = winner.generation;
        }
        put(winner.value);
        // A smaller replacement is frozen for the next run, in the SAME memory slot.
        if (next(value))
            tree.replace_winner(SelectionKey{current + (value < winner.value), value});
        else
            tree.replace_winner(std::nullopt); // EOF retires a slot; remaining slots still compete.
    }
    if (active)
        end();
}
} // namespace external::detail
