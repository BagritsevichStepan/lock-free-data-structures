#ifndef LOCK_FREE_DATA_STRUCTURES_UNBOUNDED_LOCK_FREE_STACK_H
#define LOCK_FREE_DATA_STRUCTURES_UNBOUNDED_LOCK_FREE_STACK_H

#include "concurrent_stack.h"
#include "atomic_shared_ptr/atomic_shared_ptr.h"

namespace concurrent::stack {

    template<typename T>
    class UnboundedLockFreeStack {
    private:
        struct Node {
        public:
            LFStructs::SharedPtr<Node> prev_;
            T data_;
        };

    public:
        UnboundedLockFreeStack() = default;

        UnboundedLockFreeStack(const UnboundedLockFreeStack&) = delete;
        UnboundedLockFreeStack(UnboundedLockFreeStack&&) = delete;
        UnboundedLockFreeStack& operator=(const UnboundedLockFreeStack&) = delete;
        UnboundedLockFreeStack& operator=(UnboundedLockFreeStack&&) = delete;

        [[nodiscard]] bool IsEmpty() const;

        void Push(const T& element);
        void Push(T&& element);

        bool Pop(T& element);

        ~UnboundedLockFreeStack() = default;

    private:
        inline void Push(LFStructs::SharedPtr<Node>& new_head);

    private:
        LFStructs::AtomicSharedPtr<Node> head_;
    };

    // Implementation
    template<typename T>
    bool UnboundedLockFreeStack<T>::IsEmpty() const {
        LFStructs::FastSharedPtr<Node> top = head_.getFast();
        return !top.get();
    }

    template<typename T>
    inline void UnboundedLockFreeStack<T>::Push(LFStructs::SharedPtr<Node>& new_head) {
        do {
            new_head->prev_ = head_.get();
        } while (!head_.compareExchange(new_head->prev_.get(), std::move(new_head)));
    }

    template<typename T>
    void UnboundedLockFreeStack<T>::Push(const T& element) {
        LFStructs::SharedPtr<Node> new_head{new Node()};
        new_head->data_ = element;
        Push(new_head);
    }

    template<typename T>
    void UnboundedLockFreeStack<T>::Push(T&& element) {
        LFStructs::SharedPtr<Node> new_head{new Node()};
        new_head->data_ = std::move(element);
        Push(new_head);
    }

    template<typename T>
    bool UnboundedLockFreeStack<T>::Pop(T& element) {
        LFStructs::FastSharedPtr<Node> top = head_.getFast();
        if (!top.get()) {
            return false;
        }

        while (!head_.compareExchange(top.get(), top->prev_.copy())) {
            top = head_.getFast();
            if (!top.get()) {
                return false;
            }
        }

        element = std::move(top->data_);
        return true;
    }


} // End of namespace concurrent::stack

#endif //LOCK_FREE_DATA_STRUCTURES_UNBOUNDED_LOCK_FREE_STACK_H
