macro(add_library NAME)
    set(args ${ARGN})

    # Check if any of the argument is ENCOURAGED
    list(FIND args ENCOURAGED ENCOURAGED)
    if (ENCOURAGED LESS 0)
        set(ENCOURAGED NO)
    else (ENCOURAGED LESS 0)
        set(ENCOURAGED YES)
    endif (ENCOURAGED LESS 0)

    if (${NAME}_FOUND)
        add_definitions(-DWITH_${NAME})
        include_directories(${${NAME}_INCLUDE_DIRS})
        target_link_libraries(openttd ${${NAME}_LIBRARIES})
    else (${NAME}_FOUND)
        if (ENCOURAGED)
            message(WARNING "${NAME} not found; compiling OpenTTD without ${NAME} is strongly disencouraged")
        endif (ENCOURAGED)
    endif (${NAME}_FOUND)
endmacro()
