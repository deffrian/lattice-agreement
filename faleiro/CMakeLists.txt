cmake_minimum_required(VERSION 3.21)
project(faleiro)

set(CMAKE_CXX_STANDARD 20)

add_executable(faleiro acceptor.h main.cpp)

include_directories(../lattice)