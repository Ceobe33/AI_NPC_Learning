cmake_minimum_required(VERSION 3.12)

project(hc)

add_executable(hc main.cpp)

set(CMAKE_BUILD_TYPE Release)
# set(CMAKE_BUILD_TYPE Debug)
# target_compile_definitions(hc PRIVATE DEBUG_BUILD)
