
if(__get_version_from_git_tag)
    return()
endif()
set(__get_version_from_git_tag YES)

include(GetGitRevisionDescription)

function(git_log_author_year _year)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" log -1 --pretty=format:%ad --date=format:%Y
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE res
        OUTPUT_VARIABLE out
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(NOT res EQUAL 0)
        set(out "${out}-${res}-NOTFOUND")
    endif()

    set(${_year}
        "${out}"
        PARENT_SCOPE)
endfunction()

function(get_version_from_git_tag _prefix)
    git_describe(DESCRIBE --tags --match "${_prefix}*" --always)

    if(DESCRIBE MATCHES "^${_prefix}[0-9]+\\.[0-9]+(\\.[0-9]+)?(-[0-9]+-g[0-9a-f]+)?")
        string(REGEX REPLACE "^${_prefix}([0-9]+).*" "\\1" VERSION_MAJOR "${DESCRIBE}")
        string(REGEX REPLACE "^${_prefix}[0-9]+\\.([0-9]+).*" "\\1" VERSION_MINOR "${DESCRIBE}")

        if(DESCRIBE MATCHES "^${_prefix}[0-9]+\\.[0-9]+\\.([0-9]+).*")
            string(REGEX REPLACE "^${_prefix}[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" VERSION_PATCH "${DESCRIBE}")
        else()
            set(VERSION_PATCH "0")
        endif()

        if(DESCRIBE MATCHES "^${_prefix}[0-9]+\\.[0-9]+\\.[0-9]+-[0-9]+-g([0-9a-f]+).*")
            string(REGEX REPLACE "^${_prefix}[0-9]+\\.[0-9]+\\.[0-9]+-([0-9]+).*" "\\1" VERSION_TWEAK "${DESCRIBE}")
            string(REGEX REPLACE "^${_prefix}[0-9]+\\.[0-9]+\\.[0-9]+-[0-9]+-g([0-9a-f]+).*" "\\1" VERSION_HASH "${DESCRIBE}")
            set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}")
            set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}+p${VERSION_TWEAK}-g${VERSION_HASH}")
        else()
            get_git_head_revision(VERSION_REF VERSION_HASH)
            string(SUBSTRING "${VERSION_HASH}" 0 7  VERSION_HASH)
            set(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
            set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
        endif()
        git_log_author_year(VERSION_YEAR)
    elseif(DESCRIBE MATCHES "[0-9a-f]+")
        set(VERSION "0.0.0")
        set(VERSION_HASH "${DESCRIBE}")
        set(VERSION_FULL "0.0.0+g${DESCRIBE}")
        git_log_author_year(VERSION_YEAR)
    else()
        set(VERSION "0.0.0")
        set(VERSION_HASH "NONE")
        set(VERSION_FULL "0.0.0+nogit")
        string(TIMESTAMP VERSION_YEAR "%Y")
    endif()

    set(PROJECT_VERSION "${VERSION}" PARENT_SCOPE)
    set(PROJECT_VERSION_HASH "${VERSION_HASH}" PARENT_SCOPE)
    set(PROJECT_VERSION_FULL "${VERSION_FULL}" PARENT_SCOPE)
    set(PROJECT_VERSION_YEAR "${VERSION_YEAR}" PARENT_SCOPE)
endfunction()
