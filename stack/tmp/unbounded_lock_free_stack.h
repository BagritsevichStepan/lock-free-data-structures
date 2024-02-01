#ifndef LOCK_FREE_UNBOUNDED_LOCK_FREE_STACK_H
#define LOCK_FREE_UNBOUNDED_LOCK_FREE_STACK_H

#include <atomic>
#include <utility>

namespace concurrent::stack {

    namespace details {

        template<typename T>
        using Pointer = T*;

        using PointersCount = int16_t;
        using CountedPointer = uint64_t;


        inline constexpr uint64_t kCountedPointerAddressSize = 48;
        inline constexpr CountedPointer kCountedPointerAddressMask = (uint64_t(1) << kCountedPointerAddressSize) - uint64_t(1);

        PointersCount GetPointersCount(CountedPointer counted_pointer) {
            return static_cast<PointersCount>(counted_pointer >> kCountedPointerAddressSize);
        }

        template<typename T>
        Pointer<T> GetPointer(CountedPointer counted_pointer) {
            return reinterpret_cast<Pointer<T>>(counted_pointer & kCountedPointerAddressMask);
        }

        template<typename T>
        CountedPointer GetCountedPointer(PointersCount pointers_count, Pointer<T> pointer) {
            return (reinterpret_cast<CountedPointer>(pointer) & kCountedPointerAddressMask)
                   | (static_cast<CountedPointer>(pointers_count) << kCountedPointerAddressSize);
        }


        class AtomicCountedPointer {
        public:
            AtomicCountedPointer() = default;

            AtomicCountedPointer(const AtomicCountedPointer&) = delete;
            AtomicCountedPointer(AtomicCountedPointer&&) = delete;
            AtomicCountedPointer& operator=(const AtomicCountedPointer&) = delete;
            AtomicCountedPointer& operator=(AtomicCountedPointer&&) = delete;

            CountedPointer Load(std::memory_order memory_order = std::memory_order_acquire) {
                return counted_pointer_.load(memory_order);
            }

            void Store(CountedPointer desired, std::memory_order memory_order = std::memory_order_release) {
                counted_pointer_.store(desired, memory_order);
            }

            bool CompareExchangeWeak(CountedPointer& expected, CountedPointer desired, std::memory_order memory_order = std::memory_order_seq_cst) {
                return counted_pointer_.compare_exchange_weak(expected, desired, memory_order);
            }

            ~AtomicCountedPointer() = default;

        private:
            std::atomic<CountedPointer> counted_pointer_{0};
        };
    }



    template<typename T>
    class UnboundedLockFreeStack {
    private:
        //todo
        struct Node {
            Node* next_;
            T data_;
            std::atomic<details::PointersCount> pointers_count_;
        };

    public:
        void Push(T& element) {

        }

        bool Pop(T& element) {
            details::CountedPointer top = top_.Load();
            details::CountedPointer next_top;

            T* element_pointer{nullptr};

            do {
                const Node* pointer = details::GetPointer<Node>(top);
                element_pointer = &pointer->data_;


                details::PointersCount pointers_count = details::GetPointersCount(top);
                next_top = details::SetPointersCount(top, ++pointers_count);
            } while (!top_.CompareExchangeWeak(top, next_top));


        }

    private:
        details::AtomicCountedPointer top_; // количество плюсов и адрес head
    };

}

#endif //LOCK_FREE_UNBOUNDED_LOCK_FREE_STACK_H
