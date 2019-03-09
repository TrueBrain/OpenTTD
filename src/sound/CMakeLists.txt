cmake_minimum_required(VERSION 3.5)

# Header Files
add_files(
    null_s.h
    sdl_s.h
    win32_s.h
    xaudio2_s.h
)
if (NOT WIN32)
    add_files(cocoa_s.h)
endif (NOT WIN32)

# Drivers
add_files(sound_driver.hpp)

# Sound
add_files(null_s.cpp)
if (NOT OPTION_DEDICATED)
    if (ALLEGRO_FOUND)
        add_files(allegro_s.cpp)
    endif (ALLEGRO_FOUND)
    if (SDL_FOUND)
        add_files(sdl_s.cpp)
    endif (SDL_FOUND)
    if (WIN32)
        add_files(win32_s.cpp)
        if (XAUDIO_FOUND)
            add_files(xaudio2_s.cpp)
        endif (XAUDIO_FOUND)
    endif (WIN32)
endif (NOT OPTION_DEDICATED)
if (APPLE AND OPTION_COCOA)
    add_files(cocoa_s.cpp)
endif (APPLE AND OPTION_COCOA)
