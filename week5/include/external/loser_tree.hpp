#pragma once
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace external {
// Reusable tournament: change only the current winner, then replay its O(log K) path.
// Each internal node stores the LOSER, while winner_ stores the overall winner.
template <class T, class Less = std::less<T>> class LoserTree {
  public:
    explicit LoserTree(std::vector<std::optional<T>> values, Less less = Less{})
        : values_(std::move(values)), less_(std::move(less)) {
        while (base_ < values_.size())
            base_ *= 2;
        losers_.assign(base_, values_.size());
        winner_ = build(1);
    }
    bool empty() const {
        return winner_ == values_.size() || !values_[winner_];
    }
    const T &top() const {
        if (empty())
            throw std::out_of_range("empty loser tree");
        return *values_[winner_];
    }
    std::size_t winner_index() const {
        if (empty())
            throw std::out_of_range("empty loser tree");
        return winner_;
    }
    void replace_winner(std::optional<T> value) {
        if (empty())
            throw std::out_of_range("replace on empty loser tree");
        auto challenger = winner_;
        values_[challenger] = std::move(value);
        for (auto node = (base_ + challenger) / 2; node; node /= 2) {
            auto opponent = losers_[node];
            if (better(opponent, challenger)) {
                losers_[node] = challenger;
                challenger = opponent;
            } else
                losers_[node] = opponent;
        }
        winner_ = challenger;
    }

  private:
    bool better(std::size_t a, std::size_t b) const {
        if (a == values_.size() || !values_[a])
            return false;
        if (b == values_.size() || !values_[b])
            return true;
        if (less_(*values_[a], *values_[b]))
            return true;
        return !less_(*values_[b], *values_[a]) && a < b; // deterministic ties
    }
    std::size_t build(std::size_t node) {
        if (node >= base_)
            return node - base_ < values_.size() ? node - base_ : values_.size();
        auto a = build(node * 2), b = build(node * 2 + 1);
        if (better(a, b)) {
            losers_[node] = b;
            return a;
        }
        losers_[node] = a;
        return b;
    }
    std::vector<std::optional<T>> values_;
    Less less_;
    std::size_t base_ = 1, winner_ = 0;
    std::vector<std::size_t> losers_;
};
} // namespace external
