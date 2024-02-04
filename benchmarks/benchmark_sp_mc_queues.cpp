#include <iostream>
#include <cassert>
#include <type_traits>
#include <vector>
#include <array>

#include "benchmark_utils.h"
#include "utils.h"
#include "bounded_sp_sc_queue.h"
#include "bounded_multicast_queue.h"

namespace concurrent::queue {

    template<typename T,
            std::size_t MessagesCount,
            std::size_t ConsumersCount,
            typename SPSCQueue = concurrent::queue::BoundedSPSCQueue<T, MessagesCount>>
    class SPSCBasedSPMCQueue {
    public:
        SPSCBasedSPMCQueue() = default;

        SPSCBasedSPMCQueue(const SPSCBasedSPMCQueue&) = delete;
        SPSCBasedSPMCQueue(SPSCBasedSPMCQueue&&) = delete;
        SPSCBasedSPMCQueue& operator=(const SPSCBasedSPMCQueue&) = delete;
        SPSCBasedSPMCQueue& operator=(SPSCBasedSPMCQueue&&) = delete;

        bool Read(std::size_t consumer, T& message);
        bool Write(std::size_t consumer, T desired_message);

        ~SPSCBasedSPMCQueue() = default;

    private:
        std::array<SPSCQueue, ConsumersCount> queues{};
    };

    template<typename T, std::size_t MessagesCount, std::size_t ConsumersCount, typename SPSCQueue>
    bool SPSCBasedSPMCQueue<T, MessagesCount, ConsumersCount, SPSCQueue>::Read(std::size_t consumer, T& message) {
        return queues[consumer].Dequeue(message);
    }

    template<typename T, std::size_t MessagesCount, std::size_t ConsumersCount, typename SPSCQueue>
    bool SPSCBasedSPMCQueue<T, MessagesCount, ConsumersCount, SPSCQueue>::Write(std::size_t consumer, T desired_message) {
        return queues[consumer].Enqueue(std::move(desired_message));
    }

}

namespace concurrent::benchmark::queue {

    struct Message {
        constexpr Message() = default;
        constexpr explicit Message(int x) : x_(x) {}
        constexpr ~Message() = default;
        int x_{0};
    };

    static_assert(std::is_trivially_destructible_v<Message>);
    static_assert(std::is_trivially_copyable_v<Message>);
    static_assert(concurrent::utils::IsTriviallyCopyableAndDestructible<Message>);


    template<std::size_t ReadersCount, std::size_t Capacity>
    void MeasureThroughput(const size_t rounds_count, std::array<int, ReadersCount + 1> cpu) {
        using namespace std::chrono_literals;
        using Message = concurrent::benchmark::queue::Message;

        const std::size_t capacity = Capacity;

        {
            std::vector<std::thread> readers;

            using Queue = concurrent::queue::BoundedMulticastQueue<Capacity, sizeof(Message), alignof(Message)>;
            using Reader = Queue::Reader;
            using Writer = Queue::Writer;

            Queue q{};

            auto start = std::chrono::steady_clock::now(); // Start measure the time

            for (int r = 0; r < ReadersCount; r++) {
                int cpu_number = cpu[r];
                readers.emplace_back([&q, rounds_count, cpu_number]() {
                    concurrent::benchmark::PinThread(cpu_number);

                    Reader reader{&q};
                    Message result{};
                    for (int i = 0; i < rounds_count * capacity; i++) {
                        while (!reader.Read(result)) {
                            concurrent::wait::Wait();
                        }
                        //assert(reader.Read(result));
                        assert(result.x_ == i);
                    }
                });
            }

            concurrent::benchmark::PinThread(cpu[ReadersCount]);
            Writer writer{&q};
            Message message{};
            for (int i = 0; i < rounds_count * capacity; i++) {
                message.x_ = i;
                writer.Write(message);
            }

            for (int r = 0; r < ReadersCount; r++) {
                readers[r].join();
            }

            auto stop = std::chrono::steady_clock::now(); // Stop measure the time

            std::cout << "Throughput of the concurrent::queue::BoundedMulticastQueue: " << std::endl;
            std::cout << concurrent::benchmark::GetThroughput(capacity * rounds_count, start, stop) << " ops/ms" << std::endl;
        }

        {
            std::vector<std::thread> readers;

            using Queue = concurrent::queue::SPSCBasedSPMCQueue<Message, Capacity, ReadersCount>;

            Queue q{};

            auto start = std::chrono::steady_clock::now(); // Start measure the time

            for (int r = 0; r < ReadersCount; r++) {
                int cpu_number = cpu[r];
                readers.emplace_back([&q, capacity, rounds_count, cpu_number, r]() {
                    concurrent::benchmark::PinThread(cpu_number);

                    std::this_thread::sleep_for(1ms);

                    Message result{};
                    for (int i = 0; i < rounds_count * capacity; i++) {
                        while (!q.Read(r, result));
                        assert(result.x_ == i);
                    }
                });
            }

            concurrent::benchmark::PinThread(cpu[ReadersCount]);
            Message message{};
            for (int i = 0; i < rounds_count * capacity; i++) {
                for (size_t r = 0; r < ReadersCount; r++) {
                    message.x_ = i;
                    while (!q.Write(r, message));
                }
            }

            for (int r = 0; r < ReadersCount; r++) {
                readers[r].join();
            }

            auto stop = std::chrono::steady_clock::now(); // Stop measure the time

            std::cout << "Throughput of the concurrent::queue::SPSCBasedSPMCQueue: " << std::endl;
            std::cout << concurrent::benchmark::GetThroughput(capacity * rounds_count, start, stop) << " ops/ms" << std::endl;
        }
    }

}

int main() {
    const size_t readers_count = 3;
    std::array<int, readers_count + 1> cpu = {0, 1, 2, 3};

    const size_t rounds_count = 10;
    const size_t capacity = 10000;

    concurrent::benchmark::queue::MeasureThroughput<readers_count, capacity>(rounds_count, cpu);
    return 0;
}