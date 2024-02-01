#include <vector>
#include <thread>

#include "bounded_mp_mc_queue.h"

int main() {
    std::vector<std::thread> consumers;
    std::vector<std::thread> producers;

    const std::size_t capacity = 400;
    concurrent::queue::BoundedMPMCQueue<int, capacity> q;
    for (int t = 0; t < 3; t++) {
        consumers.emplace_back([&q]() {
            int result = 0;
            for (int i = 0; i < 100; i++) {
                q.Dequeue(result);
            }
        });

        producers.emplace_back([&q]() {
            for (int i = 0; i < 100; i++) {
                q.Emplace(i);
            }
        });
    }

    for (int t = 0; t < 3; t++) {
        producers[t].join();
        consumers[t].join();
    }
    return 0;
}
