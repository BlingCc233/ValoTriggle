cmake_minimum_required(VERSION 3.23)
project(source C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(source
        helper.c
        helper.h
        main.c)
