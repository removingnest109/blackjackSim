cmake_minimum_required(VERSION 4.2)
project(blackjackSim)

set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG -Wall -Werror")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(blackjackSim main.cpp)
