set(CMAKE_AS_ARM_COMPILER_ENV_VAR "AS_ARM")
configure_file(${CMAKE_MODULE_PATH}/CMakeAS_ARMCompiler.cmake.in ${CMAKE_PLATFORM_INFO_DIR}/CMakeAS_ARMCompiler.cmake)
include(${CMAKE_ROOT}/Modules/Compiler/ARMClang.cmake)
__compiler_armclang(AS_ARM)
