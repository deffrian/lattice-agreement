cmake_minimum_required(VERSION 3.21)
project(lattice_agreement)

set(CMAKE_CXX_STANDARD 20)

add_library(asio INTERFACE)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
target_include_directories(asio INTERFACE ${CMAKE_SOURCE_DIR}/asio-1.22.1/include)
target_link_libraries(asio INTERFACE pthread)
add_executable(lattice_agreement main.cpp)

target_link_libraries(lattice_agreement asio)

include_directories(.)


add_subdirectory(faleiro)
add_subdirectory(zheng)
add_subdirectory(generalizer)
add_subdirectory(coordinator)
add_subdirectory(coordinator_generalized)
add_subdirectory(faleiro_generalized)
