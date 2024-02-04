#ifndef LOCK_FREE_DATA_STRUCTURES_UTILS_H
#define LOCK_FREE_DATA_STRUCTURES_UTILS_H

#include <cstddef>
#include <type_traits>

namespace concurrent::utils {

    inline constexpr std::size_t kDefaultAlignment = alignof(std::max_align_t);

    template<typename T>
    constexpr bool IsTriviallyCopyableAndDestructible = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

    template<typename T>
    concept IsTriviallyCopyable = std::is_trivially_copyable_v<T>;

    template<auto Number>
    concept IsEven = !(Number & 1);

} // End of namespace concurrent

#endif //LOCK_FREE_DATA_STRUCTURES_UTILS_H
