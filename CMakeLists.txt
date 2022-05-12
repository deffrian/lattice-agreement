cmake_minimum_required(VERSION 3.21)
project(lattice_agreement)

set(CMAKE_CXX_STANDARD 20)

add_executable(lattice_agreement main.cpp)

include_directories(.)

add_subdirectory(faleiro)
add_subdirectory(zheng)
add_subdirectory(generalizer)
add_subdirectory(coordinator)
add_subdirectory(coordinator_generalized)
add_subdirectory(faleiro_generalized)
