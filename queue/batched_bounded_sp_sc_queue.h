#ifndef LOCK_FREE_BATCHED_BOUNDED_SP_SC_QUEUE_H
#define LOCK_FREE_BATCHED_BOUNDED_SP_SC_QUEUE_H

#include <memory>
#include <cmath>
#include <atomic>
#include <type_traits>
#include <array>
#include <cassert>
#include <utility>
#include <climits>
#include <cstring>

#include "cache_line.h"

namespace concurrent::queue {

    namespace details {

        inline constexpr std::size_t kDefaultSlotSize = 16;

        template<typename T, std::size_t Size = kDefaultSlotSize>
        class SPSCQueueSlot {
        public:
            T* Front();

            template<typename... Args>
            void Emplace(Args&&... args);

            void Dequeue();

            bool IsEmpty();
            bool IsFull();

        private:
            T buffer_[Size];
            std::size_t head_{0};
            std::size_t tail_{0};

        };

    }

    template<typename T, std::size_t Capacity>
    class BatchedBoundedSPSCQueue final {
    public:
        BatchedBoundedSPSCQueue() = default;

        BatchedBoundedSPSCQueue(const BatchedBoundedSPSCQueue&) = delete;
        BatchedBoundedSPSCQueue(BatchedBoundedSPSCQueue&&) = delete;
        BatchedBoundedSPSCQueue& operator=(const BatchedBoundedSPSCQueue&) = delete;
        BatchedBoundedSPSCQueue& operator=(BatchedBoundedSPSCQueue&&) = delete;

        T* Front();

        template<typename... Args>
        bool Emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>);

        template<typename = std::enable_if_t<std::is_copy_constructible_v<T>, bool>>
        bool Enqueue(const T& element) noexcept(std::is_nothrow_copy_constructible_v<T>);

        template<typename = std::enable_if_t<std::is_move_constructible_v<T>, bool>>
        bool Enqueue(T&& element) noexcept(std::is_nothrow_move_constructible_v<T>);

        bool Dequeue();

        bool IsEmptyConsumer();
        std::size_t GetCapacity() const noexcept;

        ~BatchedBoundedSPSCQueue() = default;

    private:
        static constexpr std::size_t GetBufferSize();
        static constexpr std::size_t GetIndexMask();

        bool MoveHead(std::size_t& head);

    private:
        PADDING(padding0_, 0);

        details::SPSCQueueSlot<T> buffer_[GetBufferSize()];

        PADDING(padding1_, 0);

        alignas(concurrent::cache::kCacheLineSize) std::atomic<std::size_t> tail_{0};
        std::size_t cached_head_{0};

        PADDING(padding2_, sizeof(std::atomic<std::size_t>) + sizeof(std::size_t));

        alignas(concurrent::cache::kCacheLineSize) std::atomic<std::size_t> head_{0};
        std::size_t cached_tail_{0};

        PADDING(padding3_, 0);
    };


    // Implementation

    template<typename T, std::size_t Capacity>
    template<typename... Args>
    bool BatchedBoundedSPSCQueue<T, Capacity>::Emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
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
    bool BatchedBoundedSPSCQueue<T, Capacity>::Enqueue(const T& element) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        return Emplace(element);
    }

    template<typename T, std::size_t Capacity>
    template<typename>
    bool BatchedBoundedSPSCQueue<T, Capacity>::Enqueue(T&& element) noexcept(std::is_nothrow_move_constructible_v<T>) {
        return Emplace(std::forward<T>(element));
    }

    template<typename T, std::size_t Capacity>
    T* BatchedBoundedSPSCQueue<T, Capacity>::Front() {
        std::size_t head = head_.load(std::memory_order_acquire);

        if (buffer_[head].IsEmpty()) { //todo
            if (!MoveHead(head)) {
                return nullptr;
            }
        }

        return buffer_[head].Front();
    }

    template<typename T, std::size_t Capacity>
    bool BatchedBoundedSPSCQueue<T, Capacity>::Dequeue() {
        std::size_t head = head_.load(std::memory_order_acquire);

        if (buffer_[head].IsEmpty()) {
            if (!MoveHead(head)) {
                return false;
            }
        }

        buffer_[head].Dequeue();
        return true;
    }

    template<typename T, std::size_t Capacity>
    bool BatchedBoundedSPSCQueue<T, Capacity>::MoveHead(std::size_t& head) {
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
    bool BatchedBoundedSPSCQueue<T, Capacity>::IsEmptyConsumer() {
        return !Front();
    }

    template<typename T, std::size_t Capacity>
    std::size_t BatchedBoundedSPSCQueue<T, Capacity>::GetCapacity() const noexcept {
        return GetBufferSize();
    }


    template<typename T, std::size_t Capacity>
    constexpr std::size_t BatchedBoundedSPSCQueue<T, Capacity>::GetBufferSize() {
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
    constexpr std::size_t BatchedBoundedSPSCQueue<T, Capacity>::GetIndexMask() {
        return GetBufferSize() - 1;
    }


    namespace details {

        template<typename T, std::size_t Size>
        template<typename... Args>
        void SPSCQueueSlot<T, Size>::Emplace(Args &&... args) {
            new (&buffer_[tail_]) T(std::forward<Args>(args)...);
            tail_++;
        }

        template<typename T, std::size_t Size>
        void SPSCQueueSlot<T, Size>::Dequeue() {
            head_++;
        }

        template<typename T, std::size_t Size>
        T* SPSCQueueSlot<T, Size>::Front() {
            return &buffer_[head_];
        }

        template<typename T, std::size_t Size>
        bool SPSCQueueSlot<T, Size>::IsEmpty() {
            return head_ == tail_;
        }

        template<typename T, std::size_t Size>
        bool SPSCQueueSlot<T, Size>::IsFull() {
            return tail_ == Size;
        }

    }

} // End of namespace concurrent::queue

#endif //LOCK_FREE_BATCHED_BOUNDED_SP_SC_QUEUE_H

//g++ -std=c++20 SPSCQueue.h batched_bounded_sp_sc_queue.h bounded_sp_sc_queue.h benchmarks.cpp