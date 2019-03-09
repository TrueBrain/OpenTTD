cmake_minimum_required(VERSION 3.5)

add_subdirectory(npf)
add_subdirectory(opf)
add_subdirectory(yapf)

# Pathfinder
add_files(
    follow_track.hpp
    pathfinder_func.h
    pathfinder_type.h
    pf_performance_timer.hpp
)
