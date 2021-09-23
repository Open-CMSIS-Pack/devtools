cmake_minimum_required(VERSION 3.16.3)

SET(SOURCE_FILES src/lib2.cpp )

add_library(lib2 STATIC ${SOURCE_FILES})

target_include_directories(lib2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
