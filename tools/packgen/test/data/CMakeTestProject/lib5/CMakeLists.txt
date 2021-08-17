cmake_minimum_required(VERSION 3.16.3)

SET(SOURCE_FILES src/lib5.cpp )

add_library(lib5 STATIC ${SOURCE_FILES})

target_include_directories(lib5 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
