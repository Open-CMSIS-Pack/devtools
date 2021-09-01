cmake_minimum_required(VERSION 3.16.3)

SET(SOURCE_FILES src/lib3.cpp )

add_library(lib3 STATIC ${SOURCE_FILES})

target_include_directories(lib3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
