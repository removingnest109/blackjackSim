cmake_minimum_required(VERSION 4.2)
project(blackjackSim)

set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD 11)

add_compile_options(-Wall -Werror)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto -DNDEBUG")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(blackjackSim src/main.cpp
        src/config.h
        src/config.cpp
        src/stats.h
        src/print.h
        src/print.cpp
        src/model.cpp
        src/model.h
        src/actions.cpp
        src/actions.h
        src/interactive.cpp
        src/interactive.h
        src/blackjack.cpp
        src/blackjack.h
        src/simulation.cpp
        src/simulation.h
        src/cli.cpp
        src/cli.h)
