cmake_minimum_required(VERSION 3.21)
project(faleiro)

set(CMAKE_CXX_STANDARD 20)

find_package(Threads REQUIRED)

add_executable(faleiro acceptor.h main.cpp)

target_link_libraries(faleiro asio)

if(THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(faleiro PUBLIC "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(faleiro "${CMAKE_THREAD_LIBS_INIT}")
endif()

include_directories(..)