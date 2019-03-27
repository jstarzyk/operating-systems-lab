cmake_minimum_required(VERSION 3.10)
project(zad2 C)

set(CMAKE_C_STANDARD 11)

add_executable(client client.c common.h)
add_executable(server server.c common.h)
target_link_libraries(server pthread)