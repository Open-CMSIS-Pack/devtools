cmake_minimum_required(VERSION 3.16.3)

# Set CMake variables for skipping compiler identification
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_FORCED "${CMAKE_C_COMPILER_FORCED}")
set(CMAKE_CXX_COMPILER_WORKS "${CMAKE_C_COMPILER_WORKS}")

project(testproject)

SET(SOURCE_FILES ../lib1/src/lib1.cpp )

add_library(lib1 STATIC ${SOURCE_FILES})

target_include_directories(lib1 PUBLIC ../lib1/inc)
target_compile_definitions(lib1 PUBLIC "INC_GLOBAL_TEST=0x01")