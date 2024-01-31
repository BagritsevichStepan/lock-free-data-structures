project(lock_free_data_structures_tests)

set(TEST_LOCK_TARGET test_locks)
set(TEST_QUEUE_TARGET test_queues)
set(TEST_STACK_TARGET test_stacks)

add_executable(TEST_LOCK_TARGET test_locks.cpp)
add_executable(TEST_QUEUE_TARGET test_queues.cpp)
add_executable(TEST_STACK_TARGET test_stacks.cpp)

target_include_directories(TEST_LOCK_TARGET PRIVATE ${LOCK_DIRECTORIES})
target_include_directories(TEST_QUEUE_TARGET PRIVATE ${QUEUE_DIRECTORIES})
target_include_directories(TEST_STACK_TARGET PRIVATE ${STACK_DIRECTORIES})
