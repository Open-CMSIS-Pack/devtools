find_package(Doxygen 1.8.6 EXACT OPTIONAL_COMPONENTS dot)

if(DOXYGEN_FOUND)

SET(DOXYGEN_PROJECT_LOGO ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxygen_Templates/CMSIS_Logo_Final.png)
SET(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen/output)
SET(DOXYGEN_LAYOUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxygen_Templates/DoxygenLayout_forUser.xml)
SET(DOXYGEN_INPUT . src/General.txt src/CmdLineBuild.txt src/cprj_schema.txt)
SET(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cbuildgen/config)
SET(DOXYGEN_IMAGE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Build/src/images)
SET(DOXYGEN_HTML_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxygen_Templates/cmsis_header.html)
SET(DOXYGEN_HTML_FOOTER ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxygen_Templates/cmsis_footer.html)
SET(DOXYGEN_HTML_EXTRA_STYLESHEET ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxygen_Templates/cmsis.css)
SET(DOXYGEN_HTML_EXTRA_FILES Doxygen_Templates/tabs.css Doxygen_Templates/tab_topnav.png Build/example/MyProject.zip)

list(TRANSFORM DOXYGEN_INPUT PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Build/)
list(TRANSFORM DOXYGEN_HTML_EXTRA_FILES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/)

string(REPLACE ";"  " " DOXYGEN_INPUT "${DOXYGEN_INPUT}")
string(REPLACE ";"  " " DOXYGEN_HTML_EXTRA_FILES "${DOXYGEN_HTML_EXTRA_FILES}")

string(TIMESTAMP PROJECT_TIMESTAMP UTC)

configure_file(doxygen/Build/Build.dxy.in doxygen/Build/Build.dxy)
configure_file(doxygen/Doxygen_Templates/cmsis.js.in ${DOXYGEN_OUTPUT_DIRECTORY}/html/cmsis.js)

add_custom_target(buildmgr-docs
    COMMAND ${DOXYGEN_EXECUTABLE} doxygen/Build/Build.dxy
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/index.html ${DOXYGEN_OUTPUT_DIRECTORY}/index.html
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxygen_Templates/search.css ${DOXYGEN_OUTPUT_DIRECTORY}/html/search/search.css)

else()

message(WARNING "Documentation target for buildmgr not created!")

endif()
