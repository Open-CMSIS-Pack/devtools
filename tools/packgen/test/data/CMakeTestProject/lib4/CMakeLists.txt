cmake_minimum_required(VERSION 3.16.3)

SET(SOURCE_FILES src/lib4.cpp )

add_library(lib4 STATIC ${SOURCE_FILES})

target_include_directories(lib4 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
