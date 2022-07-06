function(get_all_targets var)
    set(targets)
    get_all_targets_recursive(targets ${CMAKE_CURRENT_SOURCE_DIR})
    set(${var} ${targets} PARENT_SCOPE)
endfunction()

macro(get_all_targets_recursive targets dir)
    get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirectories})
        get_all_targets_recursive(${targets} ${subdir})
    endforeach()

    get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    list(APPEND ${targets} ${current_targets})
endmacro()

function(get_targets)
  # prepare list of targets
  get_all_targets(all_targets)

  # sort target name alphabetically
  list(SORT all_targets COMPARE STRING CASE INSENSITIVE)
  list(TRANSFORM all_targets PREPEND "  -- ")
  list(TRANSFORM all_targets APPEND "\n")
  list(INSERT all_targets 0 "CMake Targets ${CMAKE_BINARY_DIR}\n")
  string (REPLACE ";" "" all_targets "${all_targets}")

  # write to file
  file(WRITE ${CMAKE_BINARY_DIR}/targets "${all_targets}")
endfunction()

