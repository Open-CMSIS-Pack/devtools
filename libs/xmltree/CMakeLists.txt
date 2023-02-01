project(XmlTree VERSION 1.0.0)

add_subdirectory("test")

SET(SOURCE_FILES AbstractFormatter.cpp JsonFormatter.cpp XmlFormatter.cpp XmlItem.cpp XMLTree.cpp)
SET(HEADER_FILES AbstractFormatter.h JsonFormatter.h XmlFormatter.h XMLTree.h XmlTreeItem.h XmlTreeItemBuilder.h
  IXmlItemBuilder.h XmlItem.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_library(XmlTree STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET XmlTree PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(XmlTree PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(XmlTree RteUtils)
