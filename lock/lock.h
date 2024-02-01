#ifndef LOCK_FREE_LOCK_H
#define LOCK_FREE_LOCK_H

#include "cache_line.h"

namespace concurrent::lock {

    template <typename Derived>
    class alignas(concurrent::cache::kCacheLineSize) Lock {
    public:
        Lock() = default;

        void lock();

        void unlock();

        virtual ~Lock() = default;
    };

    template<typename Derived>
    void Lock<Derived>::lock() {
        static_cast<Derived*>(this)->Lock();
    }

    template<typename Derived>
    void Lock<Derived>::unlock() {
        static_cast<Derived*>(this)->Unlock();
    }


}

#endif //LOCK_FREE_LOCK_H
