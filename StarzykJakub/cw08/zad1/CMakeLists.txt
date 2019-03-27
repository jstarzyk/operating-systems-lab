cmake_minimum_required(VERSION 3.10)
project(zad1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -O3")

add_executable(main main.c)
add_executable(gen_filter gen_filter.c)
target_link_libraries(main LINK_PUBLIC m pthread)
