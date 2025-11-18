cmake_minimum_required(VERSION 3.15)
project(falcon-exchange VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for performance
if(MSVC)
    add_compile_options(/W4 /O2 /DNDEBUG)
else()
    add_compile_options(-Wall -Wextra -O3 -march=native -DNDEBUG)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Source files
set(SOURCES
    src/order.hpp
    src/price_level.hpp
    src/order_book.hpp
    src/event_queue.hpp
    src/matching_engine.hpp
    src/market_data_publisher.hpp
    src/main.cpp
)

# Main executable
add_executable(falcon-exchange ${SOURCES})

# Tests
enable_testing()
add_executable(test_order_book tests/test_order_book.cpp)
target_link_libraries(test_order_book)

add_executable(test_matching_engine tests/test_matching_engine.cpp)
target_link_libraries(test_matching_engine)

# Benchmarks
add_executable(bench_throughput benchmarks/bench_throughput.cpp)
target_link_libraries(bench_throughput)

# Add tests to CTest
add_test(NAME OrderBookTest COMMAND test_order_book)
add_test(NAME MatchingEngineTest COMMAND test_matching_engine)

