cmake_minimum_required(VERSION 3.14)

project(YmlSchemaChecker VERSION 1.0.0)

add_subdirectory("test")

SET(SOURCE_FILES YmlSchemaChecker.cpp YmlSchemaErrorHandler.h YmlSchemaErrorHandler.cpp
  YmlSchemaValidator.h YmlSchemaValidator.cpp)
SET(HEADER_FILES YmlSchemaChecker.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(YmlSchemaChecker STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET YmlSchemaChecker PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(YmlSchemaChecker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(YmlSchemaChecker PUBLIC yaml-cpp nlohmann_json_schema_validator RteUtils)
