#ifndef LOCK_FREE_SPIN_LOCK_H
#define LOCK_FREE_SPIN_LOCK_H

#include <atomic>
#include <thread>

#include "lock.h"
#include "wait.h"

namespace concurrent::lock {

    class alignas(concurrent::cache::kCacheLineSize) SpinLock final : public Lock<SpinLock> {
    public:
        SpinLock() = default;

        SpinLock(const SpinLock& other) = delete;
        SpinLock(SpinLock&& other) = delete;
        SpinLock& operator=(const SpinLock& other) = delete;
        SpinLock& operator=(SpinLock&& other) = delete;

        void Lock();
        bool TryLock();
        void Unlock();

        ~SpinLock() = default;

    private:
        std::atomic<bool> locked_{false};
    };

    void SpinLock::Lock() {
        while (true) {
            if (!locked_.exchange(true, std::memory_order_release)) {
                return;
            }
            while (locked_.load(std::memory_order_acquire)) {
                concurrent::wait::Wait();
            }
        }
    }

    bool SpinLock::TryLock() {
        return !locked_.load(std::memory_order_acquire) && !locked_.exchange(true, std::memory_order_release);
    }

    void SpinLock::Unlock() {
        locked_.store(false, std::memory_order_release);
    }

} // End of namespace concurrent::lock

#endif //LOCK_FREE_SPIN_LOCK_H
