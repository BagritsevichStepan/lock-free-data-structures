#ifndef LOCK_FREE_DATA_STRUCTURES_SEQ_LOCK_H
#define LOCK_FREE_DATA_STRUCTURES_SEQ_LOCK_H

#include <atomic>
#include <utility>
#include <type_traits>

#include "utils.h"
#include "lock.h"
#include "wait.h"
#include "cache_line.h"
#include "atomic_memcpy.h"

namespace concurrent::lock {

    template<typename T>
    requires utils::IsTriviallyCopyable<T>
    class alignas(concurrent::cache::kCacheLineSize) SeqLockAtomic;

    class alignas(concurrent::cache::kCacheLineSize) SeqLock {
    public:
        using Counter = uint32_t;

        SeqLock() = default;

        SeqLock(const SeqLock&) = delete;
        SeqLock(SeqLock&&) = delete;
        SeqLock& operator=(const SeqLock&) = delete;
        SeqLock& operator=(SeqLock&&) = delete;

        Counter Load(std::memory_order memory_order = std::memory_order_seq_cst);

        Counter Lock();
        void Unlock(Counter seq);

        ~SeqLock() = default;

        static bool IsLocked(Counter seq);

    private:
        std::atomic<Counter> seq_{0};
    };

    template<typename T>
    requires utils::IsTriviallyCopyable<T>
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
    requires utils::IsTriviallyCopyable<T>
    SeqLockAtomic<T>::SeqLockAtomic(T data) : data_(std::move(data)) {}

    template<typename T>
    requires utils::IsTriviallyCopyable<T>
    T SeqLockAtomic<T>::Load() {
        T loaded;
        SeqLock::Counter seq0;
        SeqLock::Counter seq1;

        do {
            seq0 = seq_lock_.Load(std::memory_order_acquire);

            memcpy::atomic_memcpy_load(&loaded, &data_, sizeof(loaded));
            std::atomic_thread_fence(std::memory_order_acquire);

            seq1 = seq_lock_.Load(std::memory_order_relaxed);
        } while (SeqLock::IsLocked(seq0) || seq0 != seq1);

        return loaded;
    }

    template<typename T>
    requires utils::IsTriviallyCopyable<T>
    void SeqLockAtomic<T>::Store(const T& desired) {
        SeqLock::Counter seq = seq_lock_.Lock();

        std::atomic_thread_fence(std::memory_order_release);
        memcpy::atomic_memcpy_store(&data_, &desired, sizeof(desired));

        seq_lock_.Unlock(seq);
    }

    // SeqLock
    SeqLock::Counter SeqLock::Load(std::memory_order memory_order) {
        return seq_.load(memory_order);
    }

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

} // End of namespace concurrent::lock

#endif //LOCK_FREE_DATA_STRUCTURES_SEQ_LOCK_H