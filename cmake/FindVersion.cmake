macro(find_version)
    execute_process(COMMAND ./findversion.sh
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE FIND_VERSION_RESULT
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (FIND_VERSION_RESULT)
        string(REPLACE "\t" ";" FIND_VERSION ${FIND_VERSION_RESULT})
        list(GET FIND_VERSION 0 REV_VERSION)
        list(GET FIND_VERSION 1 REV_ISODATE)
        list(GET FIND_VERSION 2 REV_MODIFIED)
        list(GET FIND_VERSION 3 REV_HASH)
        list(GET FIND_VERSION 4 REV_ISTAG)

        message(STATUS "Version string: ${REV_VERSION}")
    else (FIND_VERSION_RESULT)
        message(WARNING "No version detected; this build will NOT be network compatible")
        set(REV_VERSION "norev0000")
        set(REV_ISODATE "19700101")
        set(REV_MODIFIED 1)
        set(REV_HASH "unknown")
        set(REV_ISTAG 0)
    endif (FIND_VERSION_RESULT)
endmacro()
