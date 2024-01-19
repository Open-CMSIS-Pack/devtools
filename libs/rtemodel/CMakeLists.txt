project(RteModel VERSION 1.0.0)

add_subdirectory("test")

SET(SOURCE_FILES CprjFile.cpp RteBoard.cpp RteCallback.cpp RteComponent.cpp RteCondition.cpp
  RteDevice.cpp RteExample.cpp RteFile.cpp RteGenerator.cpp RteInstance.cpp RteItem.cpp
  RteKernel.cpp RteModel.cpp RtePackage.cpp RteProject.cpp RteCprjProject.cpp
  RteTarget.cpp RteCprjTarget.cpp  RteValueAdjuster.cpp RteItemBuilder.cpp)
SET(HEADER_FILES CprjFile.h RteBoard.h  RteCallback.h RteItem.h RteKernel.h RteModel.h
  RtePackage.h RteProject.h RteCprjProject.h  RteTarget.h RteCprjTarget.h RteValueAdjuster.h
  RteComponent.h RteCondition.h RteDevice.h RteExample.h RteFile.h RteGenerator.h RteInstance.h
  RteKernelSlim.h RteItemBuilder.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(RteModel STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET RteModel PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(RteModel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(RteModel RteFsUtils RteUtils XmlTree YmlTree CrossPlatform)
