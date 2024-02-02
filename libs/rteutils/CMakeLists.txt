project(RteUtils VERSION 1.0.0)

add_subdirectory("test")

SET(SOURCE_FILES AlnumCmp.cpp CollectionUtils.cpp DeviceVendor.cpp RteConstants.cpp RteError.cpp RteUtils.cpp VersionCmp.cpp WildCards.cpp)
SET(HEADER_FILES AlnumCmp.h CollectionUtils.h DeviceVendor.h RteConstants.h RteError.h RteUtils.h ISchemaChecker.h VersionCmp.h WildCards.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(RteUtils STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET RteUtils PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(RteUtils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(RteUtils)
