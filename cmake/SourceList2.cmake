set_property(GLOBAL PROPERTY source_list)
function(add_files)
    get_property(SOURCE_FILES2 GLOBAL PROPERTY source_list)
    foreach(FILE ${ARGV})
        if (NOT "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
            list(APPEND SOURCE_FILES2 ${CMAKE_CURRENT_SOURCE_DIR}/${FILE})
        else()
            list(APPEND SOURCE_FILES2 ${tmp} ${CMAKE_SOURCE_DIR}/src/${FILE})
        endif()
    endforeach()
    set_property(GLOBAL PROPERTY source_list "${SOURCE_FILES2}")
endfunction(add_files)

function(CheckSourceEquality)
	get_property(SOURCE_FILES2 GLOBAL PROPERTY source_list)

    foreach(FILE ${SOURCE_FILES2})
        list(FIND SOURCE_FILES "${FILE}" FOUND)
        if(FOUND LESS 0)
            message(STATUS "${FILE} only in SOURCE_FILES2")
        endif(FOUND LESS 0)
    endforeach()

    foreach(FILE ${SOURCE_FILES})
        list(FIND SOURCE_FILES2 "${FILE}" FOUND)
        if(FOUND LESS 0)
            message(STATUS "${FILE} only in SOURCE_FILES")
        endif(FOUND LESS 0)
    endforeach()
endfunction(CheckSourceEquality)

set(OPTION_COCOA NO)
if (APPLE)
    set(OPTION_COCOA YES)
endif (APPLE)

# These OSes used to be supported by OpenTTD, but have no CMake code yet
set(OPTION_BEOS NO)
set(OPTION_DOS NO)
set(OPTION_MORPHOS NO)
set(OPTION_OS2 NO)

# Source Files
add_files(strgen/strgen_base.cpp)

# Header Files
add_files(strgen/strgen.h)

# Pathfinder
add_files(pathfinder/follow_track.hpp)
add_files(
    pathfinder/opf/opf_ship.cpp
    pathfinder/opf/opf_ship.h
)
add_files(
    pathfinder/pathfinder_func.h
    pathfinder/pathfinder_type.h
    pathfinder/pf_performance_timer.hpp
)

# NPF
add_files(
    pathfinder/npf/aystar.cpp
    pathfinder/npf/aystar.h
    pathfinder/npf/npf.cpp
    pathfinder/npf/npf_func.h
    pathfinder/npf/queue.cpp
    pathfinder/npf/queue.h
)

# YAPF
add_files(
    pathfinder/yapf/nodelist.hpp
    pathfinder/yapf/yapf.h
    pathfinder/yapf/yapf.hpp
    pathfinder/yapf/yapf_base.hpp
    pathfinder/yapf/yapf_cache.h
    pathfinder/yapf/yapf_common.hpp
    pathfinder/yapf/yapf_costbase.hpp
    pathfinder/yapf/yapf_costcache.hpp
    pathfinder/yapf/yapf_costrail.hpp
    pathfinder/yapf/yapf_destrail.hpp
    pathfinder/yapf/yapf_node.hpp
    pathfinder/yapf/yapf_node_rail.hpp
    pathfinder/yapf/yapf_node_road.hpp
    pathfinder/yapf/yapf_node_ship.hpp
    pathfinder/yapf/yapf_rail.cpp
    pathfinder/yapf/yapf_road.cpp
    pathfinder/yapf/yapf_ship.cpp
    pathfinder/yapf/yapf_type.hpp
)
