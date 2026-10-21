#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>

namespace external {
// Values are transferred by move; a consumer owns a block after pop returns.
template <class T> class BoundedQueue {
  public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (!capacity)
            throw std::invalid_argument("zero queue capacity");
    }
    void push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        changed_.wait(lock, [&] { return cancelled_ || closed_ || queue_.size() < capacity_; });
        if (cancelled_)
            throw std::runtime_error("pipeline cancelled");
        if (closed_)
            throw std::logic_error("push after close");
        queue_.push_back(std::move(value));
        if (queue_.size() > high_water_)
            high_water_ = queue_.size();
        changed_.notify_all();
    }
    bool pop(T &value) {
        std::unique_lock<std::mutex> lock(mutex_);
        changed_.wait(lock, [&] { return cancelled_ || closed_ || !queue_.empty(); });
        if (cancelled_)
            throw std::runtime_error("pipeline cancelled");
        if (queue_.empty())
            return false; // closed and drained: normal EOF
        value = std::move(queue_.front());
        queue_.pop_front();
        changed_.notify_all();
        return true;
    }
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        changed_.notify_all();
    }
    void cancel() {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = true;
        changed_.notify_all();
    }
    std::size_t high_water() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return high_water_;
    }

  private:
    std::size_t capacity_, high_water_ = 0;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<T> queue_;
    bool closed_ = false, cancelled_ = false;
};
} // namespace external
