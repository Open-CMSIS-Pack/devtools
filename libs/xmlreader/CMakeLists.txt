project(XmlReader VERSION 1.0.0)

add_subdirectory("test")

SET(SOURCE_FILES XML_Reader_Msgs.cpp XML_Reader.cpp)
SET(HEADER_FILES XML_Reader.h XML_InputSourceReaderFile.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(XmlReader STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET XmlReader PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(XmlReader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(XmlReader ErrLog)
