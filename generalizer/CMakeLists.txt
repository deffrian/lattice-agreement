cmake_minimum_required(VERSION 3.21)
project(generalizer)

set(CMAKE_CXX_STANDARD 20)

find_package(Threads REQUIRED)

add_executable(generalizer main.cpp)

if(THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(generalizer PUBLIC "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(generalizer "${CMAKE_THREAD_LIBS_INIT}")
endif()

include_directories(../general)