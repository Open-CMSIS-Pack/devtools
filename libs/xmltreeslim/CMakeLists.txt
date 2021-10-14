project(XmlTreeSlim VERSION 1.0.0)

add_subdirectory("test")

SET(SOURCE_FILES XMLTreeSlim.cpp XMLTreeSlimString.cpp XmlTreeSlimInterface.cpp XmlTreeSlimInterface.h)
SET(HEADER_FILES XMLTreeSlim.h XMLTreeSlimString.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(XmlTreeSlim STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET XmlTreeSlim PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(XmlTreeSlim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(XmlTreeSlim ErrLog XmlTree XmlReader)
