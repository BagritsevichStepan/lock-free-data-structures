cmake_minimum_required(VERSION 3.9)
project(lock_free_data_structures)

set(CMAKE_CXX_STANDARD 20)

include(conanbuildinfo.cmake)
conan_basic_setup()

set(LOCK_DIRECTORY ${CMAKE_SOURCE_DIR}/lock/)
set(QUEUE_DIRECTORY ${CMAKE_SOURCE_DIR}/queue/)
set(STACK_DIRECTORY ${CMAKE_SOURCE_DIR}/stack/)
set(UTILS_DIRECTORY ${CMAKE_SOURCE_DIR}/utils/)

list(APPEND LOCK_DIRECTORIES ${LOCK_DIRECTORY} ${UTILS_DIRECTORY})
list(APPEND QUEUE_DIRECTORIES ${QUEUE_DIRECTORY} ${UTILS_DIRECTORY})
list(APPEND STACK_DIRECTORIES ${STACK_DIRECTORY} ${LOCK_DIRECTORY} ${UTILS_DIRECTORY})

add_subdirectory(benchmarks/)
add_subdirectory(tests/)
