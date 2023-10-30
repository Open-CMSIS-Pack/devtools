project(YmlTree VERSION 1.0.0)

add_subdirectory("test")

SET(SOURCE_FILES YmlTree.cpp YmlFormatter.cpp YmlTreeParserInterface.cpp YmlTreeParserInterface.h)
SET(HEADER_FILES YmlTree.h YmlFormatter.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(YmlTree STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET YmlTree PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(YmlTree PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(YmlTree XmlTree RteFsUtils RteUtils yaml-cpp)
