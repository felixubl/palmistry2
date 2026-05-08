cmake_minimum_required(VERSION 3.16)

project(pokereval LANGUAGES CXX)

option(POKEREVAL_NATIVE "Build tools with -march=native when available" ON)

add_library(pokereval INTERFACE)
target_include_directories(pokereval INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(pokereval INTERFACE cxx_std_17)

add_executable(pokereval_check tools/check.cpp)
target_link_libraries(pokereval_check PRIVATE pokereval)

add_executable(pokereval_benchmark bench/benchmark.cpp)
target_link_libraries(pokereval_benchmark PRIVATE pokereval)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND POKEREVAL_NATIVE)
    target_compile_options(pokereval_check PRIVATE -march=native)
    target_compile_options(pokereval_benchmark PRIVATE -march=native)
endif()
