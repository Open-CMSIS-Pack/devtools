cmake_minimum_required(VERSION 3.16.3)

SET(SOURCE_FILES src/lib1.cpp inc/lib1.h inc)

add_library(lib1 STATIC ${SOURCE_FILES})

target_include_directories(lib1 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
target_compile_definitions(lib1 PUBLIC "INC_GLOBAL_TEST=0x01")
