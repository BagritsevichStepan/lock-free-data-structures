#ifndef LOCK_FREE_DATA_STRUCTURES_HUGE_PAGES_H
#define LOCK_FREE_DATA_STRUCTURES_HUGE_PAGES_H

#include <sys/mman.h>

#define MMAP_ACCESS (PROT_READ | PROT_WRITE)
#define MMAP_TYPE (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)


namespace concurrent::allocator {


    namespace details::huge_page_allocator {

        inline constexpr std::size_t kHugePageSize = 1u << 21u;

    }

    template<typename T>
    class HugePageAllocator {
    public:
        using value_type = T;
        using pointer = T*;

        HugePageAllocator() = default;

        template <typename U>
        HugePageAllocator(const HugePageAllocator<U>&) noexcept;
        HugePageAllocator(HugePageAllocator&&) = delete;
        HugePageAllocator& operator=(const HugePageAllocator&);
        HugePageAllocator& operator=(HugePageAllocator&&) = delete;

        pointer allocate(size_t n);
        void deallocate(pointer pointer, size_t n);

    private:
        static size_t GetHugePageSize(size_t n);

    };

    template <typename T>
    bool operator==(const HugePageAllocator<T>&, const HugePageAllocator<T>&);

    template <typename T>
    bool operator!=(const HugePageAllocator<T>&, const HugePageAllocator<T>&);


    // Implementation
    template<typename T>
    template<typename U>
    HugePageAllocator<T>::HugePageAllocator(const HugePageAllocator<U> &) noexcept {}

    template<typename T>
    HugePageAllocator<T>& HugePageAllocator<T>::operator=(const HugePageAllocator &) {
        return *this;
    }

    template<typename T>
    HugePageAllocator<T>::pointer HugePageAllocator<T>::allocate(size_t n) {
        auto ptr = static_cast<pointer>(mmap(nullptr, GetHugePageSize(n * sizeof(T)), MMAP_ACCESS, MMAP_TYPE, -1, 0));
        if (ptr == MAP_FAILED) {
            throw std::bad_alloc();
        }
        return ptr;
    }

    template<typename T>
    void HugePageAllocator<T>::deallocate(HugePageAllocator::pointer pointer, size_t n) {
        munmap(pointer, GetHugePageSize(n));
    }

    template<typename T>
    size_t HugePageAllocator<T>::GetHugePageSize(size_t n) {
        using namespace details::huge_page_allocator;
        return (((n - 1u) / kHugePageSize) + 1u) * kHugePageSize;
    }


    template<typename T>
    bool operator==(const HugePageAllocator<T>&, const HugePageAllocator<T>&) {
        return true;
    }

    template<typename T>
    bool operator!=(const HugePageAllocator<T>&, const HugePageAllocator<T>&) {
        return false;
    }

}

#endif //LOCK_FREE_DATA_STRUCTURES_HUGE_PAGES_H
