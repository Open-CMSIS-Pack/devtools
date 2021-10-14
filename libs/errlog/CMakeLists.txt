project(ErrLog VERSION 1.0.0)

add_subdirectory("test")

SET(SOURCE_FILES ErrLog.cpp ErrLog_Msgs.cpp)
SET(HEADER_FILES ErrLog.h ErrOutputter.h ErrOutputterSaveToStdoutOrFile.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(ErrLog STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET ErrLog PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(ErrLog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(ErrLog)
