cmake_minimum_required(VERSION 3.5)

add_subdirectory(cocoa)

# Header Files
add_files(
    dedicated_v.h
    null_v.h
    sdl_v.h
    win32_v.h
)
# Drivers
add_files(video_driver.hpp)

# Video
add_files(
    dedicated_v.cpp
    null_v.cpp
)
if (NOT OPTION_DEDICATED)
    if (ALLEGRO_FOUND)
        add_files(allegro_v.cpp)
    endif (ALLEGRO_FOUND)
    if (SDL_FOUND)
        add_files(sdl_v.cpp)
    endif (SDL_FOUND)
    if (WIN32)
        add_files(win32_v.cpp)
    endif (WIN32)
endif (NOT OPTION_DEDICATED)
