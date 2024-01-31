#include <iostream>
#include <thread>
#include <cassert>
#include <boost/lockfree/spsc_queue.hpp>
//#include <folly/ProducerConsumerQueue.h>

#include "benchmark_utils.h"

#include "batched_bounded_sp_sc_queue.h"
#include "bounded_sp_sc_queue.h"

int main() {
    int cpu1 = 0;
    int cpu2 = 1;

    const size_t queueSize = 100000;
    const int64_t iterations = 100000;

    {
        concurrent::queue::BoundedSPSCQueue<int, queueSize> q{};
        auto t = std::thread([&] {
            concurrent::benchmark::PinThread(cpu2);
            for (int i = 0; i < iterations; ++i) {
                while (q.IsEmptyConsumer());
                q.Dequeue();
            }
        });

        concurrent::benchmark::PinThread(cpu2);

        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < iterations; ++i) {
            q.Emplace(i);
        }
        t.join();

        auto stop = std::chrono::steady_clock::now();

        std::cout << "Throughput of the concurrent::queue::BoundedSPSCQueue:" << std::endl;
        std::cout << concurrent::benchmark::GetThroughput(iterations, start, stop) << " ops/ms" << std::endl;
    }

    {
        concurrent::queue::BoundedSPSCQueue<int, queueSize> q1{}, q2{};

        auto t = std::thread([&] {
            concurrent::benchmark::PinThread(cpu1);
            for (int i = 0; i < iterations; ++i) {
                while (q1.IsEmptyConsumer());
                q2.Emplace(*q1.Front());
                q1.Dequeue();
            }
        });

        concurrent::benchmark::PinThread(cpu2);

        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < iterations; ++i) {
            q1.Emplace(i);
            while (q2.IsEmptyConsumer());
            q2.Dequeue();
        }

        t.join();

        auto stop = std::chrono::steady_clock::now();

        std::cout << "Latency of the concurrent::queue::BoundedSPSCQueue:" << std::endl;
        std::cout << concurrent::benchmark::GetLatency(iterations, start, stop) << " ns RTT" << std::endl;
    }


    {
        boost::lockfree::spsc_queue<int> q(queueSize);
        auto t = std::thread([&] {
            concurrent::benchmark::PinThread(cpu1);
            for (int i = 0; i < iterations; ++i) {
                int val;
                while (q.pop(&val, 1) != 1);
            }
        });

        concurrent::benchmark::PinThread(cpu2);

        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < iterations; ++i) {
            while (!q.push(i));
        }

        t.join();

        auto stop = std::chrono::steady_clock::now();

        std::cout << "Throughput of the boost::lockfree::spsc_queue:" << std::endl;
        std::cout << concurrent::benchmark::GetThroughput(iterations, start, stop) << " ops/ms" << std::endl;
    }

    {
        boost::lockfree::spsc_queue<int> q1(queueSize), q2(queueSize);
        auto t = std::thread([&] {
            concurrent::benchmark::PinThread(cpu1);
            for (int i = 0; i < iterations; ++i) {
                int val;
                while (q1.pop(&val, 1) != 1);
                while (!q2.push(val));
            }
        });

        concurrent::benchmark::PinThread(cpu2);

        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) {
            while (!q1.push(i));
            int val;
            while (q2.pop(&val, 1) != 1);
        }

        t.join();

        auto stop = std::chrono::steady_clock::now();

        std::cout << "Latency of the boost::lockfree::spsc_queue:" << std::endl;
        std::cout << concurrent::benchmark::GetLatency(iterations, start, stop) << " ns RTT" << std::endl;
    }



    /*{
        folly::ProducerConsumerQueue<int> q(queueSize);
        auto t = std::thread([&] {
            concurrent::benchmark::PinThread(cpu1);
            for (int i = 0; i < iterations; ++i) {
                int val;
                while (!q.read(val));
            }
        });

        concurrent::benchmark::PinThread(cpu2);

        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < iterations; ++i) {
            while (!q.write(i));
        }

        t.join();

        auto stop = std::chrono::steady_clock::now();

        std::cout << "Throughput of the folly::ProducerConsumerQueue:" << std::endl;
        std::cout << concurrent::benchmark::GetThroughput(iterations, start, stop) << " ops/ms" << std::endl;
    }

    {
        folly::ProducerConsumerQueue<int> q1(queueSize), q2(queueSize);
        auto t = std::thread([&] {
            concurrent::benchmark::PinThread(cpu1);
            for (int i = 0; i < iterations; ++i) {
                int val;
                while (!q1.read(val));
                q2.write(val);
            }
        });

        concurrent::benchmark::PinThread(cpu2);

        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < iterations; ++i) {
            while (!q1.write(i));
            int val;
            while (!q2.read(val));
        }

        t.join();

        auto stop = std::chrono::steady_clock::now();

        std::cout << "Latency of the folly::ProducerConsumerQueue:" << std::endl;
        std::cout << concurrent::benchmark::GetLatency(iterations, start, stop) << " ns RTT" << std::endl;
    }*/

    return 0;
}
