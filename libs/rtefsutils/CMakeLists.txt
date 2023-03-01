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

# Explicitly link with stdc++fs for GCC 8
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(RteFsUtils PUBLIC RteUtils CrossPlatform stdc++fs)
else()
  target_link_libraries(RteFsUtils PUBLIC RteUtils CrossPlatform)
endif()
