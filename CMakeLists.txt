cmake_minimum_required(VERSION 4.2)
project(blackjackSim)

set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD 11)

add_compile_options(-Wall -Werror)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto -DNDEBUG")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(blackjackSim main.cpp
        config.h
        config.cpp
        stats.h
        print.h
        print.cpp
        model.cpp
        model.h
        actions.cpp
        actions.h
        interactive.cpp
        interactive.h
        blackjack.cpp
        blackjack.h
        simulation.cpp
        simulation.h
        cli.cpp
        cli.h)
