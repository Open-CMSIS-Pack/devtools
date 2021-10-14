project(RteFsUtils VERSION 1.0.0)

add_subdirectory("test")

SET(SOURCE_FILES RteFsUtils.cpp)
SET(HEADER_FILES RteFsUtils.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(RteFsUtils STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET RteFsUtils PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(RteFsUtils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(NOT EMSCRIPTEN AND
   ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
    "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
# Add library to support std::filesystem
  target_link_libraries(RteFsUtils PUBLIC RteUtils stdc++fs)
else()
  target_link_libraries(RteFsUtils PUBLIC RteUtils)
endif()

