project(SVDGenerator VERSION 1.0.0)

# add_subdirectory("test")

SET(SOURCE_FILES SvdGenerator.cpp CodeGenerator.cpp FileIo.cpp
  HeaderData.cpp HeaderData_Cluster.cpp
  HeaderData_EnumValues.cpp HeaderData_Field.cpp HeaderData_Peripheral.cpp
  HeaderData_PosMask.cpp HeaderData_Register.cpp HeaderData_RegStructure.cpp
  HeaderGenerator.cpp MemoryMap.cpp PartitionData.cpp SfdData.cpp
  SfdData_SingleItems.cpp SfdGenerator.cpp SfrccInterface.cpp)
SET(HEADER_FILES FileIo.h HeaderData.h CodeGenerator.h HeaderGenAPI.h HeaderGenerator.h
  MemoryMap.h PartitionData.h SfdData.h SfdGenAPI.h SfdGenerator.h
  SfrccInterface.h SvdGenerator.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(SVDGenerator STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET SVDGenerator PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(SVDGenerator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                                        PRIVATE ../SVDConv/include)

target_link_libraries(SVDGenerator PUBLIC ErrLog SVDModel RteFsUtils RteUtils fmt)
