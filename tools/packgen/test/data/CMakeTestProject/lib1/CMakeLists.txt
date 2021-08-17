cmake_minimum_required(VERSION 3.16.3)

SET(SOURCE_FILES src/lib1.cpp )

add_library(lib1 STATIC ${SOURCE_FILES})

target_include_directories(lib1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
