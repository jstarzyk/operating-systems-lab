cmake_minimum_required(VERSION 3.10)
project(zad1 C)

set(CMAKE_C_STANDARD 11)

add_library(libbarber_shop SHARED libbarber_shop.c)

add_executable(barber barber.c libbarber_shop.h)
add_executable(customer customer.c libbarber_shop.h)
target_link_libraries(barber LINK_PUBLIC libbarber_shop)
target_link_libraries(customer LINK_PUBLIC libbarber_shop)