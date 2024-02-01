#ifndef LOCK_FREE_DATA_STRUCTURES_SEQ_LOCK_H
#define LOCK_FREE_DATA_STRUCTURES_SEQ_LOCK_H

#include <atomic>
#include <utility>
#include <type_traits>
#include <boost/atomic.hpp>

#include "lock.h"
#include "wait.h"

namespace concurrent::lock {

    namespace details {

        template<typename T>
        concept IsTriviallyCopyable = std::is_trivially_copyable_v<T>;

        using MaxBitsType = uintmax_t;

        template<typename T>
        void atomic_memcpy_load(void* dest, const void* src, std::size_t from, std::size_t to);

        void atomic_memcpy_load(void* dest, const void* src, std::size_t count);

        template<typename T>
        void atomic_memcpy_store(void* dest, const void* src, std::size_t from, std::size_t to);

        void atomic_memcpy_store(void* dest, const void* src, std::size_t count);
    }

    template<typename T>
    requires details::IsTriviallyCopyable<T>
    class alignas(concurrent::cache::kCacheLineSize) SeqLockAtomic;

    class alignas(concurrent::cache::kCacheLineSize) SeqLock {
    public:
        using Counter = uint32_t;

        SeqLock() = default;

        SeqLock(const SeqLock&) = delete;
        SeqLock(SeqLock&&) = delete;
        SeqLock& operator=(const SeqLock&) = delete;
        SeqLock& operator=(SeqLock&&) = delete;

        Counter Lock();
        void Unlock(Counter seq);

        ~SeqLock() = default;

        template<typename T>
        requires details::IsTriviallyCopyable<T>
        friend class SeqLockAtomic;

    private:
        static bool IsLocked(Counter seq);

        std::atomic<Counter> seq_{0};
    };

    template<typename T>
    requires details::IsTriviallyCopyable<T>
    class alignas(concurrent::cache::kCacheLineSize) SeqLockAtomic {
    public:
        explicit SeqLockAtomic(T data);

        SeqLockAtomic(const SeqLockAtomic&) = delete;
        SeqLockAtomic(SeqLockAtomic&&) = delete;
        SeqLockAtomic& operator=(const SeqLockAtomic&) = delete;
        SeqLockAtomic& operator=(SeqLockAtomic&&) = delete;

        T Load();
        void Store(const T& desired);

        ~SeqLockAtomic() = default;

    private:
        SeqLock seq_lock_{};
        T data_;
    };


    // Implementation
    template<typename T>
    requires details::IsTriviallyCopyable<T>
    SeqLockAtomic<T>::SeqLockAtomic(T data) : data_(std::move(data)) {}

    template<typename T>
    requires details::IsTriviallyCopyable<T>
    T SeqLockAtomic<T>::Load() {
        T loaded;
        SeqLock::Counter seq0;
        SeqLock::Counter seq1;

        do {
            seq0 = seq_lock_.seq_.load(std::memory_order_acquire);

            details::atomic_memcpy_load(&loaded, &data_, sizeof(loaded));
            std::atomic_thread_fence(std::memory_order_acquire);

            seq1 = seq_lock_.seq_.load(std::memory_order_relaxed);
        } while (SeqLock::IsLocked(seq0) || seq0 != seq1);

        return loaded;
    }

    template<typename T>
    requires details::IsTriviallyCopyable<T>
    void SeqLockAtomic<T>::Store(const T& desired) {
        SeqLock::Counter seq = seq_lock_.Lock();

        std::atomic_thread_fence(std::memory_order_release);
        details::atomic_memcpy_store(&data_, &desired, sizeof(desired));

        seq_lock_.Unlock(seq);
    }

    // SeqLock
    SeqLock::Counter SeqLock::Lock() {
        Counter seq = seq_.load(std::memory_order_relaxed);

        while (true) {
            while (IsLocked(seq)) {
                concurrent::wait::Wait();
                seq = seq_.load(std::memory_order_relaxed);
            }

            if (seq_.compare_exchange_weak(seq, seq + 1U, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        }

        return seq;
    }

    void SeqLock::Unlock(Counter seq) {
        seq_.store(seq + 2U, std::memory_order_release);
    }

    bool SeqLock::IsLocked(Counter seq) {
        return seq & 1U;
    }

    // details
    namespace details {

        void atomic_memcpy_load(void* dest, const void* src, std::size_t count) {
            const std::size_t max_bits_type_count = count / sizeof(MaxBitsType);
            const std::size_t max_bits_type_bytes_count = max_bits_type_count * sizeof(MaxBitsType);

            atomic_memcpy_load<MaxBitsType>(dest, src, 0, max_bits_type_count);
            atomic_memcpy_load<char>(dest, src, max_bits_type_bytes_count, count);
        }

        template<typename T>
        void atomic_memcpy_load(void* dest, const void* src, std::size_t from, std::size_t to) {
            for (std::size_t i = from; i < to; ++i) {
                static_cast<T*>(dest)[i] = boost::atomic_ref<const T>(static_cast<const T*>(src)[i])
                        .load(boost::memory_order_relaxed);
            }
        }

        void atomic_memcpy_store(void* dest, const void* src, std::size_t count) {
            const std::size_t max_bits_type_count = count / sizeof(MaxBitsType);
            const std::size_t max_bits_type_bytes_count = max_bits_type_count * sizeof(MaxBitsType);

            atomic_memcpy_store<MaxBitsType>(dest, src, 0, max_bits_type_count);
            atomic_memcpy_store<char>(dest, src, max_bits_type_bytes_count, count);
        }

        template<typename T>
        void atomic_memcpy_store(void* dest, const void* src, std::size_t from, std::size_t to) {
            for (std::size_t i = from; i < to; ++i) {
                boost::atomic_ref<T>(static_cast<T*>(dest)[i])
                        .store(static_cast<const T*>(src)[i], boost::memory_order_relaxed);
            }
        }

    }
} // End of namespace concurrent::lock

#endif //LOCK_FREE_DATA_STRUCTURES_SEQ_LOCK_H