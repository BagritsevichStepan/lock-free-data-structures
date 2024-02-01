#include <iostream>
#include <optional>
#include <array>
#include <string>
#include <vector>
#include <boost/lockfree/stack.hpp>

#include "benchmark_utils.h"

#include "unbounded_locked_stack.h"
#include "unbounded_lock_free_stack.h"
#include "alternative_stack/lfstack.h"


namespace concurrent::benchmark::stacks {

    template<typename Stack, std::size_t CpuNumber, typename... Args>
    requires concurrent::benchmark::IsEven<CpuNumber>
    void MeasureThroughput(const IterationsCount iterations, std::array<int, CpuNumber> cpu, const std::string& stack_name, Args&&... args) {
        Stack stack{std::forward<Args>(args)...};

        std::vector<std::thread> consumers;
        std::vector<std::thread> producers;

        for (int i = 0; i < CpuNumber / 2; ++i) {
            int cpu_number = cpu[i];
            consumers.emplace_back([&stack, iterations, cpu_number] {
                concurrent::benchmark::PinThread(cpu_number);
                int result = 0;
                for (int i = 0; i < iterations; ++i) {
                    while (!stack.Pop(result));
                }
            });
        }

        auto start = std::chrono::steady_clock::now(); // Start measure the time

        for (int i = CpuNumber / 2; i < CpuNumber - 1; ++i) {
            int cpu_number = cpu[i];
            producers.emplace_back([&stack, iterations, cpu_number] {
                concurrent::benchmark::PinThread(cpu_number);
                for (int i = 0; i < iterations; ++i) {
                    stack.Push(i);
                }
            });
        }

        concurrent::benchmark::PinThread(cpu.back());
        for (int i = 0; i < iterations; ++i) {
            stack.Push(i);
        }

        for (int i = 0; i < producers.size(); ++i) {
            producers[i].join();
        }

        for (int i = 0; i < consumers.size(); ++i) {
            consumers[i].join();
        }

        auto stop = std::chrono::steady_clock::now(); // Stop measure the time

        std::cout << "Throughput of the " << stack_name << ": " << std::endl;
        std::cout << concurrent::benchmark::GetThroughput(iterations, start, stop) << " ops/ms" << std::endl;
    }

    void MeasureLatency() {
        //todo
    }


    template<typename T, typename Stack>
    class StackAdapter {
    public:
        inline void Push(const T& element) {
            stack_.push(element);
        }

        inline bool Pop(T& element) {
            std::optional<T> x = stack_.pop();
            if (x) {
                element = x.value();
                return true;
            }
            return false;
        }

    private:
        Stack stack_{};
    };

    template<typename T>
    class StackAdapter<T, boost::lockfree::stack<T>> {
    public:
        template<typename... Args>
        StackAdapter(Args&&... args) : stack_(std::forward<Args>(args)...) {}

        inline void Push(const T& element) {
            stack_.push(element);
        }

        inline bool Pop(T& element) {
            return stack_.pop(element);
        }

    private:
        boost::lockfree::stack<T> stack_;
    };


} // End of namespace concurrent::benchmark::stacks

int main() {
    std::array<int, 2> cpu = {0, 1};

    const concurrent::benchmark::IterationsCount iterations = 100000;

    concurrent::benchmark::stacks::MeasureThroughput<concurrent::stack::UnboundedLockFreeStack<int>, cpu.size()>(
            iterations,
            cpu,
            "concurrent::stack::UnboundedLockFreeStack");

    concurrent::benchmark::stacks::MeasureThroughput<concurrent::stack::UnboundedSpinLockedStack<int>, cpu.size()>(
            iterations,
            cpu,
            "concurrent::stack::UnboundedSpinLockedStack");

    concurrent::benchmark::stacks::MeasureThroughput<concurrent::stack::UnboundedMutexLockedStack<int>, cpu.size()>(
            iterations,
            cpu,
            "concurrent::stack::UnboundedMutexLockedStack");

    using LFStack = concurrent::benchmark::stacks::StackAdapter<int, LFStructs::LFStack<int>>;
    concurrent::benchmark::stacks::MeasureThroughput<LFStack, cpu.size()>(
            iterations,
            cpu,
            "LFStructs::LFStack");

    using BoostLFStack = concurrent::benchmark::stacks::StackAdapter<int, boost::lockfree::stack<int>>;
    concurrent::benchmark::stacks::MeasureThroughput<BoostLFStack, cpu.size()>(
            iterations,
            cpu,
            "boost::lockfree::stack",
            iterations);

    return 0;
}
