cmake_minimum_required(VERSION 3.16.3)

# Set CMake variables for skipping compiler identification
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_FORCED "${CMAKE_C_COMPILER_FORCED}")
set(CMAKE_CXX_COMPILER_WORKS "${CMAKE_C_COMPILER_WORKS}")

project(testproject)

if(DEVICE STREQUAL "DEV1")
  add_library(lib STATIC src/core.cpp src/dev1.cpp)
  target_include_directories(lib PUBLIC inc inc1)
elseif(DEVICE STREQUAL "DEV2")
  add_library(lib STATIC src/core.cpp src/dev2.cpp)
  target_include_directories(lib PUBLIC inc inc2)
elseif(DEVICE STREQUAL "DEV3")
  add_library(lib STATIC src/core.cpp src/dev3.cpp)
  target_include_directories(lib PUBLIC inc inc3)
else()
  message(FATAL_ERROR "Error: DEVICE was not defined!")
endif()
