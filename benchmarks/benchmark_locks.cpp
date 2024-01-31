#include <iostream>
#include <thread>

#include "benchmark_utils.h"

#include "lock.h"
#include "seq_lock.h"
#include "spin_lock.h"



int main() {
    using namespace std::chrono_literals;

    concurrent::lock::SeqLockAtomic<int> shared_data{5};
    auto writer = std::thread([&shared_data]() {
        shared_data.Store(15);
    });
    auto reader = std::thread([&shared_data]() {
        std::this_thread::sleep_for(1ms);
        std::cout << shared_data.Load() << std::endl;
    });

    writer.join();
    reader.join();
    return 0;
}
