function (set_options)
    option(OPTION_DEDICATED "Build dedicated server only (no GUI)" NO)
    option(OPTION_NETWORK "Build with network support" YES)
    option(OPTION_USE_THREADS "Use threads" YES)
    option(OPTION_USE_ASSERTS "Use assertions; leave enabled for nightlies, betas, and RCs" YES)
    # TODO -- Support timidity
    set(OPTION_TIMIDITY NO)
endfunction (set_options)

function (show_options)
    message(STATUS "Option Dedicated - ${OPTION_DEDICATED}")
    message(STATUS "Option Network - ${OPTION_NETWORK}")
    message(STATUS "Option Threads - ${OPTION_USE_THREADS}")
    message(STATUS "Option Assert - ${OPTION_USE_ASSERTS}")
endfunction (show_options)

function (add_optional_definitions)
    if (OPTION_NETWORK)
        add_definitions(-DENABLE_NETWORK)
    endif (OPTION_NETWORK)

    if (OPTION_DEDICATED)
        add_definitions(-DDEDICATED)
    endif (OPTION_DEDICATED)

    if (NOT OPTION_USE_THREADS)
        add_definitions(-DNO_THREADS)
    endif (NOT OPTION_USE_THREADS)

    if (OPTION_USE_ASSERTS)
        add_definitions(-DWITH_ASSERT)
    else (OPTION_USE_ASSERTS)
        add_definitions(-DNDEBUG)
    endif (OPTION_USE_ASSERTS)
endfunction (add_optional_definitions)
