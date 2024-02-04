#ifndef LOCK_FREE_DATA_STRUCTURES_BENCHMARK_UTILS_H
#define LOCK_FREE_DATA_STRUCTURES_BENCHMARK_UTILS_H

#include <pthread.h>
#include <sched.h>
#include <chrono>

#include "utils.h"

namespace concurrent::benchmark {

    using IterationsCount = int64_t;
    using Time = std::chrono::time_point<std::chrono::steady_clock>;

    inline constexpr int kSuccessfullyPinnedThread = 0;
    inline constexpr int kFailedToPinThread = -1;


    int PinThread(int cpu) {
#ifdef __linux__
        if (cpu < 0) {
            std::cerr << "The CPU core number must be greater than or equal to 0" << std::endl;
            return kFailedToPinThread;
        }

        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        CPU_SET(cpu, &cpu_set);
        if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set)) {
            std::cerr << "Failed during pthread_setaffinity_np" << std::endl;
            return kFailedToPinThread;
        }

        return kSuccessfullyPinnedThread;
#else
        std::cerr << "Method pthread_setaffinity_np is not supported in the system" << std::endl;
        return kFailedToPinThread;
#endif
    }

    IterationsCount GetThroughput(IterationsCount iterations, Time start, Time stop) {
        return iterations * 1000000 / std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
    }

    IterationsCount GetLatency(IterationsCount iterations, Time start, Time stop) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count() / iterations;
    }

}

#endif //LOCK_FREE_DATA_STRUCTURES_BENCHMARK_UTILS_H
