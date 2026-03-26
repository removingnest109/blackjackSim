cmake_minimum_required(VERSION 4.2)
project(blackjack)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Werror)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto -DNDEBUG")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(blackjack_core
        src/config.cpp
        src/print.cpp
        src/model.cpp
        src/actions.cpp
        src/interactive.cpp
        src/blackjack.cpp
        src/simulation.cpp
        src/cli.cpp
)

target_include_directories(blackjack_core PUBLIC src)

add_executable(blackjack src/main.cpp)
target_link_libraries(blackjack PRIVATE blackjack_core)

include(CTest)
enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(blackjack_tests
        tests/test_blackjack.cpp
        tests/test_resolve_hand.cpp
        tests/test_card_counting.cpp
)
target_link_libraries(blackjack_tests PRIVATE blackjack_core GTest::gtest_main)
target_compile_features(blackjack_tests PRIVATE cxx_std_17)

if (TARGET gtest)
    set_target_properties(gtest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
endif ()
if (TARGET gtest_main)
    set_target_properties(gtest_main PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
endif ()

include(GoogleTest)
gtest_discover_tests(blackjack_tests)