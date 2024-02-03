#ifndef LOCK_FREE_BOUNDED_SP_SC_QUEUE_H
#define LOCK_FREE_BOUNDED_SP_SC_QUEUE_H

#include <memory>
#include <cmath>
#include <atomic>
#include <type_traits>
#include <array>
#include <cassert>
#include <utility>
#include <climits>

#include "cache_line.h"

namespace concurrent::queue {

    template<typename T, std::size_t Capacity>
    class BoundedSPSCQueue final {
    public:
        BoundedSPSCQueue() = default;

        BoundedSPSCQueue(const BoundedSPSCQueue&) = delete;
        BoundedSPSCQueue(BoundedSPSCQueue&&) = delete;
        BoundedSPSCQueue& operator=(const BoundedSPSCQueue&) = delete;
        BoundedSPSCQueue& operator=(BoundedSPSCQueue&&) = delete;

        T* Front();

        template<typename... Args>
        bool Emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>);

        template<typename = std::enable_if_t<std::is_copy_constructible_v<T>, bool>>
        bool Enqueue(const T& element) noexcept(std::is_nothrow_copy_constructible_v<T>);

        template<typename = std::enable_if_t<std::is_move_constructible_v<T>, bool>>
        bool Enqueue(T&& element) noexcept(std::is_nothrow_move_constructible_v<T>);

        bool Dequeue();

        template<typename = std::enable_if_t<std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>, bool>>
        bool Dequeue(T& element);

        [[nodiscard]] bool IsEmptyConsumer(); // IsEmpty method for consumer. It is faster than IsEmptyProducer()
        [[nodiscard]] bool IsEmptyProducer() const noexcept; // IsEmpty method for producer
        [[nodiscard]] std::size_t GetSize() const noexcept;
        [[nodiscard]] std::size_t GetCapacity() const noexcept;

        ~BoundedSPSCQueue() = default;

    private:
        static constexpr std::size_t GetBufferSize();
        static constexpr std::size_t GetIndexMask();

    private:
        PADDING(padding0_, 0);

        std::array<T, GetBufferSize()> buffer_{};

        PADDING(padding1_, 0);

        alignas(cache::kCacheLineSize) std::atomic<std::size_t> tail_{0};
        std::size_t cached_head_{0};

        PADDING(padding2_, sizeof(std::atomic<std::size_t>) + sizeof(std::size_t));

        alignas(cache::kCacheLineSize) std::atomic<std::size_t> head_{0};
        std::size_t cached_tail_{0};

        PADDING(padding3_, sizeof(std::atomic<std::size_t>) + sizeof(std::size_t));
    };


    template<typename T, std::size_t Capacity>
    T* BoundedSPSCQueue<T, Capacity>::Front() {
        std::size_t head = head_.load(std::memory_order_acquire);

        if (head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (head == cached_tail_) {
                return nullptr;
            }
        }
        return &buffer_[head];
    }

    template<typename T, std::size_t Capacity>
    template<typename... Args>
    bool BoundedSPSCQueue<T, Capacity>::Emplace(Args &&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        std::size_t next_tail = (tail + 1) & GetIndexMask();

        if (next_tail == cached_head_) {
            cached_head_ = head_.load(std::memory_order_acquire);
            if (next_tail == cached_head_) {
                return false;
            }
        }

        new (&buffer_[tail]) T(std::forward<Args>(args)...);
        tail_.store(next_tail, std::memory_order_release);

        return true;
    }

    template<typename T, std::size_t Capacity>
    template<typename>
    bool BoundedSPSCQueue<T, Capacity>::Enqueue(const T& element) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        return Emplace(element);
    }

    template<typename T, std::size_t Capacity>
    template<typename>
    bool BoundedSPSCQueue<T, Capacity>::Enqueue(T&& element) noexcept(std::is_nothrow_move_constructible_v<T>) {
        return Emplace(std::forward<T>(element));
    }

    template<typename T, std::size_t Capacity>
    bool BoundedSPSCQueue<T, Capacity>::Dequeue() {
        std::size_t head = head_.load(std::memory_order_acquire);

        if (head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (head == cached_tail_) {
                return false;
            }
        }

        head = (head + 1) & GetIndexMask();
        head_.store(head, std::memory_order_release);

        return true;
    }

    template<typename T, std::size_t Capacity>
    template<typename>
    bool BoundedSPSCQueue<T, Capacity>::Dequeue(T &element) {
        std::size_t head = head_.load(std::memory_order_acquire);

        if (head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (head == cached_tail_) {
                return false;
            }
        }

        if constexpr (std::is_move_constructible_v<T>) {
            element = std::move(buffer_[head]);
        } else {
            element = buffer_[head];
        }

        head = (head + 1) & GetIndexMask();
        head_.store(head, std::memory_order_release);

        return true;
    }

    template<typename T, std::size_t Capacity>
    bool BoundedSPSCQueue<T, Capacity>::IsEmptyConsumer() {
        return !Front();
    }

    template<typename T, std::size_t Capacity>
    bool BoundedSPSCQueue<T, Capacity>::IsEmptyProducer() const noexcept {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
    }

    template<typename T, std::size_t Capacity>
    std::size_t BoundedSPSCQueue<T, Capacity>::GetSize() const noexcept {
        std::ptrdiff_t size = tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_acquire);
        if (size < 0) {
            size += GetBufferSize();
        }
        return static_cast<std::size_t>(size);
    }

    template<typename T, std::size_t Capacity>
    std::size_t BoundedSPSCQueue<T, Capacity>::GetCapacity() const noexcept {
        return GetBufferSize();
    }


    template<typename T, std::size_t Capacity>
    constexpr std::size_t BoundedSPSCQueue<T, Capacity>::GetBufferSize() {
        std::size_t capacity = Capacity + 1;
        if (capacity < 4) {
            capacity = 4;
        }

        int max_bits_number = sizeof(std::size_t) * CHAR_BIT - 1;
        capacity = 1 << (max_bits_number - __builtin_clzll(capacity) + 1);

        assert((capacity & (capacity - 1)) == 0); // is power of two

        return capacity;
    }

    template<typename T, std::size_t Capacity>
    constexpr std::size_t BoundedSPSCQueue<T, Capacity>::GetIndexMask() {
        return GetBufferSize() - 1;
    }

} // End of namespace concurrent::queue

#endif //LOCK_FREE_BOUNDED_SP_SC_QUEUE_H
