cmake_minimum_required(VERSION 3.16)
project(Tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-pipe")
link_libraries(pthread stdc++fs)

add_executable(Tests
        usb_reader.cpp
        )
