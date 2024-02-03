#ifndef LOCK_FREE_BOUNDED_MP_MC_QUEUE_H
#define LOCK_FREE_BOUNDED_MP_MC_QUEUE_H

#include <array>
#include <cassert>
#include <utility>
#include <type_traits>

#include "cache_line.h"

namespace concurrent::queue {

    using Generation = uint32_t;

    namespace details {

        template <typename T>
        class MPMCQueueSlot {
        public:
            template<typename... Args, typename = std::enable_if_t<std::is_nothrow_constructible_v<T, Args...>, bool>>
            void Construct(Args&&... args) noexcept;

            T&& Move() noexcept;

            template<typename = std::enable_if_t<std::is_nothrow_destructible_v<T>, bool>>
            void Destroy() noexcept;

            Generation LoadGeneration(std::memory_order order = std::memory_order_acquire);
            void StoreGeneration(const Generation& new_generation, std::memory_order order = std::memory_order_release);

            ~MPMCQueueSlot();

        private:
            alignas(concurrent::cache::kCacheLineSize) std::atomic<Generation> generation_{0};
            std::aligned_storage_t<sizeof(T), alignof(T)> data_;
        };

    }

    template<typename T, std::size_t Capacity>
    class BoundedMPMCQueue {
    public:
        BoundedMPMCQueue() = default;

        BoundedMPMCQueue(const BoundedMPMCQueue&) = delete;
        BoundedMPMCQueue(BoundedMPMCQueue&&) = delete;
        BoundedMPMCQueue& operator=(const BoundedMPMCQueue&) = delete;
        BoundedMPMCQueue& operator=(BoundedMPMCQueue&&) = delete;

        template<typename... Args, typename = std::enable_if_t<std::is_nothrow_constructible_v<T, Args...>, bool>>
        void Emplace(Args&&... args) noexcept;

        template<typename... Args, typename = std::enable_if_t<std::is_nothrow_constructible_v<T, Args...>, bool>>
        bool TryEmplace(Args&&... args) noexcept;

        template<typename = std::enable_if_t<std::is_nothrow_copy_constructible_v<T>, bool>>
        void Enqueue(const T& element) noexcept;

        template<typename = std::enable_if_t<std::is_nothrow_copy_constructible_v<T>, bool>>
        bool TryEnqueue(const T& element) noexcept;

        template<typename = std::enable_if_t<std::is_nothrow_move_constructible_v<T>, bool>>
        void Enqueue(T&& element) noexcept;

        template<typename = std::enable_if_t<std::is_nothrow_move_constructible_v<T>, bool>>
        bool TryEnqueue(T&& element) noexcept;


        template<typename = std::enable_if_t<std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>, bool>>
        void Dequeue(T& element);

        template<typename = std::enable_if_t<std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>, bool>>
        bool TryDequeue(T& element);


        [[nodiscard]] std::size_t GetSize() const noexcept;
        [[nodiscard]] bool IsEmpty() const noexcept;
        [[nodiscard]] std::size_t GetCapacity() const noexcept;

        ~BoundedMPMCQueue() = default;

    private:
        std::size_t GetIndex(std::size_t i);
        Generation GetGeneration(std::size_t i);

        static constexpr std::size_t GetBufferSize();
        static constexpr std::size_t GetIndexMask();
        static constexpr std::size_t GetGenerationMask();

    private:
        PADDING(padding0_, 0);

        std::array<details::MPMCQueueSlot<T>, GetBufferSize()> buffer_;

        PADDING(padding1_, 0);

        alignas(concurrent::cache::kCacheLineSize) std::atomic<std::size_t> head_{0};
        alignas(concurrent::cache::kCacheLineSize) std::atomic<std::size_t> tail_{0};

        PADDING(padding2_, 0);
    };


    // Implementation

    namespace details {

        template<typename T>
        template<typename... Args, typename>
        void MPMCQueueSlot<T>::Construct(Args&&... args) noexcept {
            new (&data_) T(std::forward<Args>(args)...);
        }

        template<typename T>
        T&& MPMCQueueSlot<T>::Move() noexcept {
            return std::move((*reinterpret_cast<T*>(&data_)));
        }

        template<typename T>
        template<typename>
        void MPMCQueueSlot<T>::Destroy() noexcept {
            reinterpret_cast<T*>(&data_)->~T();
        }

        template<typename T>
        Generation MPMCQueueSlot<T>::LoadGeneration(std::memory_order order) {
            return generation_.load(order);
        }

        template<typename T>
        void MPMCQueueSlot<T>::StoreGeneration(const Generation& new_generation, std::memory_order order) {
            return generation_.store(new_generation, order);
        }

        template<typename T>
        MPMCQueueSlot<T>::~MPMCQueueSlot() {
            if (generation_.load() & 1) {
                Destroy();
            }
        }

    }

    template<typename T, std::size_t Capacity>
    template<typename... Args, typename>
    void BoundedMPMCQueue<T, Capacity>::Emplace(Args&&... args) noexcept {
        const std::size_t tail = tail_.fetch_add(1);

        const std::size_t index = GetIndex(tail);
        const Generation generation = 2 * GetGeneration(tail);

        while (generation != buffer_[index].LoadGeneration());

        buffer_[index].Construct(std::forward<Args>(args)...);
        buffer_[index].StoreGeneration(generation + 1);
    }

    template<typename T, std::size_t Capacity>
    template<typename... Args, typename>
    bool BoundedMPMCQueue<T, Capacity>::TryEmplace(Args&&... args) noexcept {
        std::size_t tail = tail_.load(std::memory_order_acquire);
        while (true) {
            const std::size_t index = GetIndex(tail);
            const Generation generation = 2 * GetGeneration(tail);
            if (generation == buffer_[index].LoadGeneration()) {
                if (tail_.compare_exchange_weak(tail, tail + 1)) {
                    buffer_[index].Construct(std::forward<Args>(args)...);
                    buffer_[index].StoreGeneration(generation + 1);
                    return true;
                }
            } else {
                const std::size_t new_tail = tail_.load(std::memory_order_acquire);
                if (tail == new_tail) {
                    return false;
                }
                tail = new_tail;
            }
        }
    }

    template<typename T, std::size_t Capacity>
    template<typename>
    void BoundedMPMCQueue<T, Capacity>::Enqueue(const T& element) noexcept {
        Emplace(element);
    }

    template<typename T, std::size_t Capacity>
    template<typename>
    bool BoundedMPMCQueue<T, Capacity>::TryEnqueue(const T& element) noexcept {
        return TryEmplace(element);
    }

    template<typename T, std::size_t Capacity>
    template<typename>
    void BoundedMPMCQueue<T, Capacity>::Enqueue(T&& element) noexcept {
        Emplace(std::forward<T>(element));
    }

    template<typename T, std::size_t Capacity>
    template<typename>
    bool BoundedMPMCQueue<T, Capacity>::TryEnqueue(T&& element) noexcept {
        return TryEmplace(std::forward<T>(element));
    }


    template<typename T, std::size_t Capacity>
    template<typename>
    void BoundedMPMCQueue<T, Capacity>::Dequeue(T& element) {
        const std::size_t head = head_.fetch_add(1);

        const std::size_t index = GetIndex(head);
        const Generation generation = 2 * GetGeneration(head) + 1;

        while (generation != buffer_[index].LoadGeneration());

        element = buffer_[index].Move();

        buffer_[index].Destroy();
        buffer_[index].StoreGeneration(generation + 1);
    }

    template<typename T, std::size_t Capacity>
    template<typename>
    bool BoundedMPMCQueue<T, Capacity>::TryDequeue(T& element) {
        std::size_t head = head_.load(std::memory_order_acquire);
        while (true) {
            const std::size_t index = GetIndex(head);
            const Generation generation = 2 * GetGeneration(head) + 1;
            if (generation == buffer_[index].LoadGeneration()) {
                if (head_.compare_exchange_weak(head, head + 1)) {
                    element = buffer_[index].Move();
                    buffer_[index].Destroy();
                    buffer_[index].StoreGeneration(generation + 1);
                    return true;
                }
            } else {
                const std::size_t new_head = head_.load(std::memory_order_acquire);
                if (new_head == head) {
                    return false;
                }
                head = new_head;
            }
        }
    }

    template<typename T, std::size_t Capacity>
    std::size_t BoundedMPMCQueue<T, Capacity>::GetSize() const noexcept {
        return tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_acquire);
    }

    template<typename T, std::size_t Capacity>
    bool BoundedMPMCQueue<T, Capacity>::IsEmpty() const noexcept {
        return GetSize() == 0;
    }

    template<typename T, std::size_t Capacity>
    std::size_t BoundedMPMCQueue<T, Capacity>::GetCapacity() const noexcept {
        return GetBufferSize();
    }


    template<typename T, std::size_t Capacity>
    std::size_t BoundedMPMCQueue<T, Capacity>::GetIndex(std::size_t i) {
        return i & GetIndexMask();
    }

    template<typename T, std::size_t Capacity>
    Generation BoundedMPMCQueue<T, Capacity>::GetGeneration(std::size_t i) {
        return i & GetGenerationMask();
    }

    template<typename T, std::size_t Capacity>
    constexpr std::size_t BoundedMPMCQueue<T, Capacity>::GetBufferSize() {
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
    constexpr std::size_t BoundedMPMCQueue<T, Capacity>::GetIndexMask() {
        return GetBufferSize() - 1;
    }

    template<typename T, std::size_t Capacity>
    constexpr std::size_t BoundedMPMCQueue<T, Capacity>::GetGenerationMask() {
        return ~GetIndexMask();
    }

} //End of namespace concurrent::queue

#endif //LOCK_FREE_BOUNDED_MP_MC_QUEUE_H
