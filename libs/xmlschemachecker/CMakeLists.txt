project(XmlValidator VERSION 1.0.0)

add_subdirectory("test")

SET(SOURCE_FILES XmlErrorHandler.cpp XmlValidator.cpp XmlErrorHandler.h XmlValidator.h XmlChecker.cpp)
SET(HEADER_FILES XmlChecker.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(XmlValidator STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET XmlValidator PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(XmlValidator
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_BINARY_DIR}/external/xerces-c/src
        PRIVATE ${CMAKE_SOURCE_DIR}/external/xerces-c/src
        )

target_link_libraries(XmlValidator PUBLIC ErrLog PRIVATE xerces-c)
