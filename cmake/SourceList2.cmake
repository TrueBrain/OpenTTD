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
add_files(sound/null_s.h)
add_files(video/null_v.h)
add_files(sound/sdl_s.h)
add_files(video/sdl_v.h)
add_files(strgen/strgen.h)
add_files(sound/win32_s.h)
add_files(video/win32_v.h)
add_files(sound/xaudio2_s.h)
if (NOT WIN32)
    add_files(sound/cocoa_s.h)
    add_files(
        video/cocoa/cocoa_keys.h
        video/cocoa/cocoa_v.h
    )
endif (NOT WIN32)

# Tables
add_files(
    table/airport_defaults.h
    table/airport_movement.h
    table/airporttile_ids.h
    table/airporttiles.h
    table/animcursors.h
    table/autorail.h
    table/bridge_land.h
    table/build_industry.h
    table/cargo_const.h
    table/clear_land.h
    table/control_codes.h
    table/elrail_data.h
    table/engines.h
    table/genland.h
    table/heightmap_colours.h
    table/industry_land.h
    table/landscape_sprite.h
    table/newgrf_debug_data.h
    table/object_land.h
    table/palette_convert.h
    table/palettes.h
    table/pricebase.h
    table/railtypes.h
    table/road_land.h
    table/roadveh_movement.h
    table/sprites.h
    table/station_land.h
    table/strgen_tables.h
    table/string_colours.h
    table/town_land.h
    table/townname.h
    table/track_land.h
    table/train_cmd.h
    table/tree_land.h
    table/unicode.h
    table/water_land.h
)

# Script
add_files(
    script/script_config.cpp
    script/script_config.hpp
    script/script_fatalerror.hpp
    script/script_info.cpp
    script/script_info.hpp
    script/script_info_dummy.cpp
    script/script_instance.cpp
    script/script_instance.hpp
    script/script_scanner.cpp
    script/script_scanner.hpp
    script/script_storage.hpp
    script/script_suspend.hpp
    script/squirrel.cpp
    script/squirrel.hpp
    script/squirrel_class.hpp
    script/squirrel_helper.hpp
    script/squirrel_helper_type.hpp
    script/squirrel_std.cpp
    script/squirrel_std.hpp
)

# AI API
add_files(script/api/ai_changelog.hpp)

# Game API
add_files(script/api/game_changelog.hpp)

# Script API
add_files(
    script/api/script_accounting.hpp
    script/api/script_admin.hpp
    script/api/script_airport.hpp
    script/api/script_base.hpp
    script/api/script_basestation.hpp
    script/api/script_bridge.hpp
    script/api/script_bridgelist.hpp
    script/api/script_cargo.hpp
    script/api/script_cargolist.hpp
    script/api/script_cargomonitor.hpp
    script/api/script_client.hpp
    script/api/script_clientlist.hpp
    script/api/script_company.hpp
    script/api/script_companymode.hpp
    script/api/script_controller.hpp
    script/api/script_date.hpp
    script/api/script_depotlist.hpp
    script/api/script_engine.hpp
    script/api/script_enginelist.hpp
    script/api/script_error.hpp
    script/api/script_event.hpp
    script/api/script_event_types.hpp
    script/api/script_execmode.hpp
    script/api/script_game.hpp
    script/api/script_gamesettings.hpp
    script/api/script_goal.hpp
    script/api/script_group.hpp
    script/api/script_grouplist.hpp
    script/api/script_industry.hpp
    script/api/script_industrylist.hpp
    script/api/script_industrytype.hpp
    script/api/script_industrytypelist.hpp
    script/api/script_info_docs.hpp
    script/api/script_infrastructure.hpp
    script/api/script_list.hpp
    script/api/script_log.hpp
    script/api/script_map.hpp
    script/api/script_marine.hpp
    script/api/script_news.hpp
    script/api/script_object.hpp
    script/api/script_order.hpp
    script/api/script_rail.hpp
    script/api/script_railtypelist.hpp
    script/api/script_road.hpp
    script/api/script_sign.hpp
    script/api/script_signlist.hpp
    script/api/script_station.hpp
    script/api/script_stationlist.hpp
    script/api/script_story_page.hpp
    script/api/script_storypagelist.hpp
    script/api/script_storypageelementlist.hpp
    script/api/script_subsidy.hpp
    script/api/script_subsidylist.hpp
    script/api/script_testmode.hpp
    script/api/script_text.hpp
    script/api/script_tile.hpp
    script/api/script_tilelist.hpp
    script/api/script_town.hpp
    script/api/script_townlist.hpp
    script/api/script_tunnel.hpp
    script/api/script_types.hpp
    script/api/script_vehicle.hpp
    script/api/script_vehiclelist.hpp
    script/api/script_viewport.hpp
    script/api/script_waypoint.hpp
    script/api/script_waypointlist.hpp
    script/api/script_window.hpp
)

# Script API Implementation
add_files(
    script/api/script_accounting.cpp
    script/api/script_admin.cpp
    script/api/script_airport.cpp
    script/api/script_base.cpp
    script/api/script_basestation.cpp
    script/api/script_bridge.cpp
    script/api/script_bridgelist.cpp
    script/api/script_cargo.cpp
    script/api/script_cargolist.cpp
    script/api/script_cargomonitor.cpp
    script/api/script_client.cpp
    script/api/script_clientlist.cpp
    script/api/script_company.cpp
    script/api/script_companymode.cpp
    script/api/script_controller.cpp
    script/api/script_date.cpp
    script/api/script_depotlist.cpp
    script/api/script_engine.cpp
    script/api/script_enginelist.cpp
    script/api/script_error.cpp
    script/api/script_event.cpp
    script/api/script_event_types.cpp
    script/api/script_execmode.cpp
    script/api/script_game.cpp
    script/api/script_gamesettings.cpp
    script/api/script_goal.cpp
    script/api/script_group.cpp
    script/api/script_grouplist.cpp
    script/api/script_industry.cpp
    script/api/script_industrylist.cpp
    script/api/script_industrytype.cpp
    script/api/script_industrytypelist.cpp
    script/api/script_infrastructure.cpp
    script/api/script_list.cpp
    script/api/script_log.cpp
    script/api/script_map.cpp
    script/api/script_marine.cpp
    script/api/script_news.cpp
    script/api/script_object.cpp
    script/api/script_order.cpp
    script/api/script_rail.cpp
    script/api/script_railtypelist.cpp
    script/api/script_road.cpp
    script/api/script_sign.cpp
    script/api/script_signlist.cpp
    script/api/script_station.cpp
    script/api/script_stationlist.cpp
    script/api/script_story_page.cpp
    script/api/script_storypagelist.cpp
    script/api/script_storypageelementlist.cpp
    script/api/script_subsidy.cpp
    script/api/script_subsidylist.cpp
    script/api/script_testmode.cpp
    script/api/script_text.cpp
    script/api/script_tile.cpp
    script/api/script_tilelist.cpp
    script/api/script_town.cpp
    script/api/script_townlist.cpp
    script/api/script_tunnel.cpp
    script/api/script_vehicle.cpp
    script/api/script_vehiclelist.cpp
    script/api/script_viewport.cpp
    script/api/script_waypoint.cpp
    script/api/script_waypointlist.cpp
    script/api/script_window.cpp
)

# Drivers
add_files(sound/sound_driver.hpp)
add_files(video/video_driver.hpp)

# Sprite loaders
add_files(
    spriteloader/grf.cpp
    spriteloader/grf.hpp
    spriteloader/spriteloader.hpp
)

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

# Sound
add_files(sound/null_s.cpp)
if (NOT OPTION_DEDICATED)
    if (ALLEGRO_FOUND)
        add_files(sound/allegro_s.cpp)
    endif (ALLEGRO_FOUND)
    if (SDL_FOUND)
        add_files(sound/sdl_s.cpp)
    endif (SDL_FOUND)
    if (WIN32)
        add_files(sound/win32_s.cpp)
        if (XAUDIO_FOUND)
            add_files(sound/xaudio2_s.cpp)
        endif (XAUDIO_FOUND)
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
        add_files(sound/cocoa_s.cpp)
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
