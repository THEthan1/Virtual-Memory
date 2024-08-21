cmake_minimum_required(VERSION 3.1)
project(OS4)

set(CMAKE_C_STANDARD 11)
include_directories(.)

add_executable(OS4 MemoryConstants.h PhysicalMemory.cpp PhysicalMemory.h VirtualMemory.cpp VirtualMemory.h SimpleTest.cpp)