#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace sharekeys {
// Bounded multi-producer/single-consumer queue. Producers reserve a slot with a
// CAS, then publish it using a release store. The consumer cannot overtake an
// earlier reserved slot, preserving cross-hook publication order. No allocation,
// locks, or waits in push; contention exhaustion is treated like a full queue.
template<class T, std::size_t Capacity>
class MpscQueue {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0);
    static_assert(std::is_nothrow_copy_assignable_v<T>);
    static_assert(std::atomic<std::size_t>::is_always_lock_free);
    struct Cell { std::atomic<std::size_t> sequence{}; T value{}; };
public:
    MpscQueue() { for (std::size_t i = 0; i < Capacity; ++i) cells_[i].sequence.store(i, std::memory_order_relaxed); }
    bool push(const T& value) noexcept {
        auto position = write_.load(std::memory_order_relaxed);
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
            if (position > std::numeric_limits<std::size_t>::max() - Capacity - 1) return false;
            auto& cell = cells_[position & (Capacity - 1)];
            const auto sequence = cell.sequence.load(std::memory_order_acquire);
            if (sequence == position) {
                if (write_.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) {
                    cell.value = value;
                    cell.sequence.store(position + 1, std::memory_order_release);
                    return true;
                }
            } else if (sequence < position) return false;
            else position = write_.load(std::memory_order_relaxed);
        }
        return false;
    }
    bool pop(T& value) noexcept {
        auto& cell = cells_[read_ & (Capacity - 1)];
        if (cell.sequence.load(std::memory_order_acquire) != read_ + 1) return false;
        value = cell.value;
        cell.sequence.store(read_ + Capacity, std::memory_order_release);
        ++read_; return true;
    }
private:
    std::array<Cell, Capacity> cells_{};
    alignas(64) std::atomic<std::size_t> write_{};
    alignas(64) std::size_t read_{};
};
// Exactly one producer and one consumer. Never blocks a low-level hook.
template<class T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity > 0);
public:
    bool push(const T& value) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) % (Capacity + 1);
        if (next == tail_.load(std::memory_order_acquire)) return false;
        data_[head] = value;
        head_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& value) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        value = data_[tail];
        tail_.store((tail + 1) % (Capacity + 1), std::memory_order_release);
        return true;
    }
private:
    std::array<T, Capacity + 1> data_{};
    alignas(64) std::atomic<std::size_t> head_{};
    alignas(64) std::atomic<std::size_t> tail_{};
};
}
