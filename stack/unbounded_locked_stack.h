#ifndef LOCK_FREE_UNBOUNDED_SPIN_LOCKED_STACK_H
#define LOCK_FREE_UNBOUNDED_SPIN_LOCKED_STACK_H

#include <stack>
#include <mutex>

#include "spin_lock.h"


namespace concurrent::stack {

    template<typename T, typename Lock, typename Stack = std::stack<T>>
    class UnboundedLockedStack;


    namespace details {

        template<typename T, typename DerivedLock>
        using UnboundedBaseLockedStack = UnboundedLockedStack<T, concurrent::lock::Lock<DerivedLock>>;
    }

    template<typename T>
    using UnboundedSpinLockedStack = details::UnboundedBaseLockedStack<T, concurrent::lock::SpinLock>;

    template<typename T>
    using UnboundedMutexLockedStack = UnboundedLockedStack<T, std::mutex>;


    template<typename T, typename Lock, typename Stack>
    class UnboundedLockedStack {
    public:
        UnboundedLockedStack() = default;

        UnboundedLockedStack(const UnboundedLockedStack&) = delete;
        UnboundedLockedStack(UnboundedLockedStack&&) = delete;
        UnboundedLockedStack& operator=(const UnboundedLockedStack&) = delete;
        UnboundedLockedStack& operator=(UnboundedLockedStack&&) = delete;

        [[nodiscard]] bool IsEmpty() const;

        template <typename... Args>
        void Emplace(Args&&... args);

        void Push(const T& element);
        void Push(T&& element);

        bool Pop(T& element);

        ~UnboundedLockedStack() = default;

    private:
        Stack stack_;
        Lock lock_;
    };


    template<typename T, typename Lock, typename Stack>
    bool UnboundedLockedStack<T, Lock, Stack>::IsEmpty() const {
        std::lock_guard<Lock> lock_guard{lock_};
        return stack_.empty();
    }

    template<typename T, typename Lock, typename Stack>
    template <typename... Args>
    void UnboundedLockedStack<T, Lock, Stack>::Emplace(Args&&... args) {
        std::lock_guard<Lock> lock_guard{lock_};
        return stack_.emplace(std::forward<Args>(args)...);
    }

    template<typename T, typename Lock, typename Stack>
    void UnboundedLockedStack<T, Lock, Stack>::Push(const T& element) {
        std::lock_guard<Lock> lock_guard{lock_};
        return stack_.push(element);
    }

    template<typename T, typename Lock, typename Stack>
    void UnboundedLockedStack<T, Lock, Stack>::Push(T&& element) {
        std::lock_guard<Lock> lock_guard{lock_};
        return stack_.push(std::forward<T>(element));
    }

    template<typename T, typename Lock, typename Stack>
    bool UnboundedLockedStack<T, Lock, Stack>::Pop(T& element) {
        std::lock_guard<Lock> lock_guard{lock_};
        if (!stack_.empty()) {
            element = std::move(stack_.top());
            stack_.pop();
            return true;
        }
        return false;
    }


} // End of namespace concurrent::stack

#endif //LOCK_FREE_UNBOUNDED_SPIN_LOCKED_STACK_H
