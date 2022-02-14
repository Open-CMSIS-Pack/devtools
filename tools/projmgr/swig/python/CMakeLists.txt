find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

find_package(PythonLibs)
include_directories(projmgr-python PUBLIC ${PYTHON_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${PROJECT_BINARY_DIR})
set_source_files_properties(projmgr.i PROPERTIES CPLUSPLUS ON)

swig_add_library(projmgr-python LANGUAGE python SOURCES projmgr.i)
set_target_properties(projmgr-python PROPERTIES OUTPUT_NAME "projmgr")

swig_link_libraries(projmgr-python ${PYTHON_LIBRARIES} projmgrlib)
