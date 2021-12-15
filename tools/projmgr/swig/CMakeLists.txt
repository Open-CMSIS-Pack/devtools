find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

find_package(PythonLibs)
include_directories(projmgr PUBLIC ${PYTHON_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/../include ${PROJECT_BINARY_DIR})
set_source_files_properties(projmgr.i PROPERTIES CPLUSPLUS ON)

swig_add_library(projmgr LANGUAGE python SOURCES projmgr.i)

swig_link_libraries(projmgr ${PYTHON_LIBRARIES} projmgrlib)
