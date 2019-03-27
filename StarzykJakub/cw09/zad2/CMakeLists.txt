cmake_minimum_required(VERSION 3.10)
project(zad2 C)

set(CMAKE_C_STANDARD 11)

add_executable(zad2 main.c)
target_link_libraries(zad2 pthread m)