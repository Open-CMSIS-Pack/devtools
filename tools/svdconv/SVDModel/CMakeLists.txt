project(SVDModel VERSION 1.0.0)

# add_subdirectory("test")

SET(SOURCE_FILES SvdItem.cpp SvdModel.cpp SvdPeripheral.cpp
  SvdRegister.cpp SvdSauRegion.cpp SvdTypes.cpp SvdUtils.cpp
  SvdWriteConstraint.cpp SvdAddressBlock.cpp SvdCluster.cpp SvdCpu.cpp
  SvdDerivedFrom.cpp SvdDevice.cpp SvdDimension.cpp SvdEnum.cpp SvdCExpression.cpp
  SvdCExpressionParser.cpp SvdField.cpp SvdInterrupt.cpp)
SET(HEADER_FILES SvdDevice.h SvdDimension.h SvdEnum.h SvdCExpression.h SvdCExpressionParser.h
  SvdField.h SvdInterrupt.h SvdItem.h SvdModel.h SvdPeripheral.h SvdRegister.h
  SvdSauRegion.h SvdTypes.h SvdUtils.h SvdWriteConstraint.h EnumStringTables.h
  SvdAddressBlock.h SvdCluster.h SvdCpu.h SvdDerivedFrom.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(SVDModel STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET SVDModel PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(SVDModel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(SVDModel PUBLIC ErrLog XmlTree CrossPlatform)
