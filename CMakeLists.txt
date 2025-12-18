cmake_minimum_required(VERSION 4.2)
project(blackjackSim)

set(CMAKE_CXX_STANDARD 26)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

add_executable(blackjackSim main.cpp)
