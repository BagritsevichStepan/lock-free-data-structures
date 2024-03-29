#ifndef LOCK_FREE_CACHE_LINE_H
#define LOCK_FREE_CACHE_LINE_H

#include <cstddef>

#define PADDING(padding_name, occupied_bytes_length) char padding_name[concurrent::cache::kCacheLineSize - ((occupied_bytes_length) % concurrent::cache::kCacheLineSize)]

namespace concurrent::cache {

#ifdef CACHE_LINE_SIZE
    inline constexpr std::size_t kCacheLineSize = CACHE_LINE_SIZE;
#else
#ifdef __cpp_lib_hardware_interference_size
    inline constexpr std::size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
    inline constexpr std::size_t kCacheLineSize = 64;
#endif
#endif

}

#endif //LOCK_FREE_CACHE_LINE_H
