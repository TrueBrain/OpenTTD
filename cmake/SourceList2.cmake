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
add_files(video/dedicated_v.h)
add_files(video/null_v.h)
add_files(video/sdl_v.h)
add_files(strgen/strgen.h)
add_files(video/win32_v.h)
if (NOT WIN32)
    add_files(
        video/cocoa/cocoa_keys.h
        video/cocoa/cocoa_v.h
    )
endif (NOT WIN32)

# Drivers
add_files(video/video_driver.hpp)

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

# Video
add_files(
    video/dedicated_v.cpp
    video/null_v.cpp
)
if (NOT OPTION_DEDICATED)
    if (ALLEGRO_FOUND)
        add_files(video/allegro_v.cpp)
    endif (ALLEGRO_FOUND)
    if (SDL_FOUND)
        add_files(video/sdl_v.cpp)
    endif (SDL_FOUND)
    if (WIN32)
        add_files(video/win32_v.cpp)
    endif (WIN32)
endif (NOT OPTION_DEDICATED)

if (APPLE)
# OSX Files
    if (OPTION_COCOA)
        add_files(
            video/cocoa/cocoa_v.mm
            video/cocoa/event.mm
            video/cocoa/fullscreen.mm
            video/cocoa/wnd_quartz.mm
            video/cocoa/wnd_quickdraw.mm
        )
    endif (OPTION_COCOA)
endif (APPLE)

# Threading
add_files(thread/thread.h)
if (OPTION_USE_THREADS)
    if (WIN32)
        add_files(thread/thread_win32.cpp)
    else (WIN32)
        if (OPTION_OS2)
            add_files(thread/thread_os2.cpp)
        else (OPTION_OS2)
            if (OPTION_MORPHOS)
                add_files(thread/thread_morphos.cpp)
            else (OPTION_MORPHOS)
                add_files(thread/thread_pthread.cpp)
            endif (OPTION_MORPHOS)
        endif (OPTION_OS2)
    endif (WIN32)
else (OPTION_USE_THREADS)
    add_files(thread/thread_none.cpp)
endif (OPTION_USE_THREADS)
