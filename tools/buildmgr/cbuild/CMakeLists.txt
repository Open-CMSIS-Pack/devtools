SET(SOURCE_FILES CbuildCallback.cpp CbuildProject.cpp CbuildUtils.cpp
  Cbuild.cpp CbuildLayer.cpp CbuildMsgs.cpp CbuildKernel.cpp CbuildModel.cpp)
SET(HEADER_FILES CbuildKernel.h CbuildModel.h CbuildCallback.h CbuildProject.h CbuildUtils.h
  Cbuild.h CbuildLayer.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(cbuild STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET cbuild PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(cbuild
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../cbuildgen/include)

target_link_libraries(cbuild PUBLIC CrossPlatform ErrLog RteFsUtils RteUtils XmlTree XmlTreeSlim XmlReader RteModel)
