#ifndef LOCK_FREE_DATA_STRUCTURES_WAIT_H
#define LOCK_FREE_DATA_STRUCTURES_WAIT_H

#include <thread>

#if defined(_M_X64)
#define CONCURRENT_WAIT __builtin_ia32_pause()
#elif defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define CONCURRENT_WAIT _mm_pause()
#elif defined(_M_ARM64)
#define CONCURRENT_WAIT __builtin_ia32_pause()
#else
#define CONCURRENT_WAIT std::this_thread::yield()
#endif

#ifndef SMT_ENABLED
#define SMT_ENABLED 1
#endif

namespace concurrent::wait {

    inline void Wait() {
        if constexpr (SMT_ENABLED) {
            CONCURRENT_WAIT;
        }
    }

}

#endif //LOCK_FREE_DATA_STRUCTURES_WAIT_H
