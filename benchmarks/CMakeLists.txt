project(lock_free_data_structures_benchmark)

set(BENCH_LOCK_TARGET benchmark_locks)
set(BENCH_QUEUE_TARGET benchmark_queues)
set(BENCH_STACK_TARGET benchmark_stacks)

add_executable(BENCH_LOCK_TARGET benchmark_locks.cpp)
add_executable(BENCH_SP_SC_QUEUE_TARGET benchmark_sp_sc_queues.cpp)
add_executable(BENCH_MP_MC_QUEUE_TARGET benchmark_mp_mc_queues.cpp)
add_executable(BENCH_STACK_TARGET benchmark_stacks.cpp)

set(ALTERNATIVE_STACK_DIRECTORY alternative_stack/)

target_include_directories(BENCH_LOCK_TARGET PRIVATE ${LOCK_DIRECTORIES})
target_include_directories(BENCH_SP_SC_QUEUE_TARGET PRIVATE ${QUEUE_DIRECTORIES} ${FOLLY_DIRECTORY})
target_include_directories(BENCH_MP_MC_QUEUE_TARGET PRIVATE ${QUEUE_DIRECTORIES})
target_include_directories(BENCH_STACK_TARGET PRIVATE ${STACK_DIRECTORIES})

