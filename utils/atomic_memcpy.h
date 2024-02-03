#ifndef LOCK_FREE_DATA_STRUCTURES_ATOMIC_MEMCPY_H
#define LOCK_FREE_DATA_STRUCTURES_ATOMIC_MEMCPY_H

#include <boost/atomic.hpp>

namespace concurrent::memcpy {

    using MaxBitsType = uintmax_t;

    void atomic_memcpy_load(void* dest, const void* src, std::size_t count);
    void atomic_memcpy_store(void* dest, const void* src, std::size_t count);

    namespace details {

        template<typename T>
        void atomic_memcpy_load(void* dest, const void* src, std::size_t from, std::size_t to);

        template<typename T>
        void atomic_memcpy_store(void* dest, const void* src, std::size_t from, std::size_t to);

    }


    // Implementation

    void atomic_memcpy_load(void* dest, const void* src, std::size_t count) {
        const std::size_t max_bits_type_count = count / sizeof(MaxBitsType);
        const std::size_t max_bits_type_bytes_count = max_bits_type_count * sizeof(MaxBitsType);

        details::atomic_memcpy_load<MaxBitsType>(dest, src, 0, max_bits_type_count);
        details::atomic_memcpy_load<char>(dest, src, max_bits_type_bytes_count, count);
    }

    void atomic_memcpy_store(void* dest, const void* src, std::size_t count) {
        const std::size_t max_bits_type_count = count / sizeof(MaxBitsType);
        const std::size_t max_bits_type_bytes_count = max_bits_type_count * sizeof(MaxBitsType);

        details::atomic_memcpy_store<MaxBitsType>(dest, src, 0, max_bits_type_count);
        details::atomic_memcpy_store<char>(dest, src, max_bits_type_bytes_count, count);
    }


    namespace details {

        template<typename T>
        void atomic_memcpy_load(void* dest, const void* src, std::size_t from, std::size_t to) {
            for (std::size_t i = from; i < to; ++i) {
                static_cast<T*>(dest)[i] = boost::atomic_ref<const T>(static_cast<const T*>(src)[i])
                        .load(boost::memory_order_relaxed);
            }
        }

        template<typename T>
        void atomic_memcpy_store(void* dest, const void* src, std::size_t from, std::size_t to) {
            for (std::size_t i = from; i < to; ++i) {
                boost::atomic_ref<T>(static_cast<T*>(dest)[i])
                        .store(static_cast<const T*>(src)[i], boost::memory_order_relaxed);
            }
        }

    }

} // End of namespace concurrent::memcpy

#endif //LOCK_FREE_DATA_STRUCTURES_ATOMIC_MEMCPY_H
