cmake_minimum_required(VERSION 3.26)
project(C_test C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_executable(C_test
        main.c
        src/cpu.c
)
