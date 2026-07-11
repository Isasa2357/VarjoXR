#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace VarjoXR {

// Single-producer, multi-consumer latest-value exchange.
//
// The producer publishes immutable shared objects into alternating slots. A
// consumer keeps the returned shared_ptr alive, so the producer may reuse the
// same slot later without invalidating an in-progress read. Intermediate values
// may be skipped intentionally; this class is for latest-state exchange, not for
// lossless event delivery.
template <typename T>
class AsyncDoubleBuffer {
    static_assert(!std::is_reference_v<T>,
                  "AsyncDoubleBuffer cannot store reference types.");

public:
    AsyncDoubleBuffer() = default;

    AsyncDoubleBuffer(const AsyncDoubleBuffer&) = delete;
    AsyncDoubleBuffer& operator=(const AsyncDoubleBuffer&) = delete;

    void publish(T value)
    {
        auto immutable = std::make_shared<const T>(std::move(value));
        const std::uint64_t nextGeneration =
            generation_.load(std::memory_order_relaxed) + 1u;
        const std::size_t slotIndex =
            static_cast<std::size_t>(nextGeneration & 1u);

        std::atomic_store_explicit(
            &slots_[slotIndex],
            std::move(immutable),
            std::memory_order_release);
        generation_.store(nextGeneration, std::memory_order_release);
    }

    std::shared_ptr<const T> latest() const noexcept
    {
        for (;;) {
            const std::uint64_t before =
                generation_.load(std::memory_order_acquire);
            if (before == 0u) return {};

            const std::size_t slotIndex =
                static_cast<std::size_t>(before & 1u);
            auto value = std::atomic_load_explicit(
                &slots_[slotIndex],
                std::memory_order_acquire);

            const std::uint64_t after =
                generation_.load(std::memory_order_acquire);
            if (before == after) return value;
        }
    }

    std::uint64_t generation() const noexcept
    {
        return generation_.load(std::memory_order_acquire);
    }

private:
    mutable std::shared_ptr<const T> slots_[2];
    std::atomic<std::uint64_t> generation_{0};
};

} // namespace VarjoXR
