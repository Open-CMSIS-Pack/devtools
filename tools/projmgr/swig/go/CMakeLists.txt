find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

set(CMAKE_SWIG_FLAGS -c++ -go -cgo -intgosize 64 -D${CMAKE_SYSTEM_NAME})
include_directories(projmgr-go PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${PROJECT_BINARY_DIR})
set_source_files_properties(projmgr.i PROPERTIES CPLUSPLUS ON)

swig_add_library(projmgr-go LANGUAGE go SOURCES projmgr.i)
set_target_properties(projmgr-go PROPERTIES OUTPUT_NAME "projmgr")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  target_link_options(projmgr-go PUBLIC -undefined dynamic_lookup)
endif()

swig_link_libraries(projmgr-go projmgrlib)
