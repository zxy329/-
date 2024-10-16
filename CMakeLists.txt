cmake_minimum_required(VERSION 3.22.1)

project(threadpool)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set (CMAKE_BUILD_TYPE Debug)

include_directories(${CMAKE_SOURCE_DIR})
add_executable(my_cmake_exe main.cpp threadpool.cpp)


