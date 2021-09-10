cmake_minimum_required(VERSION 3.16)
project(Tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS " -lpthread -pipe ")

add_executable(Tests
#    test.c
#    test_in.cpp
#    test_out.cpp
#    test_ping.cpp
        usb_reader.cpp
#       /home/alexander/Documents/University/Comp_architect/Parallel_computation/Lab_2/main.cpp
        )
