cmake_minimum_required(VERSION 3.5)

# Music
if (NOT OPTION_DEDICATED)
    if (ALLEGRO_FOUND)
        add_files(allegro_m.cpp)
    endif (ALLEGRO_FOUND)
    if (WIN32)
        add_files(dmusic.cpp)
    endif (WIN32)
endif (NOT OPTION_DEDICATED)
add_files(
    null_m.cpp
    midifile.cpp
)
if (NOT OPTION_DEDICATED)
    if (WIN32)
        add_files(win32_m.cpp)
    else (WIN32)
        if (NOT OPTION_DOS AND NOT OPTION_MORPHOS)
            add_files(extmidi.cpp)
        endif (NOT OPTION_DOS AND NOT OPTION_MORPHOS)
    endif (WIN32)
    if (OPTION_BEOS)
        add_files(bemidi.cpp)
    endif (OPTION_BEOS)
    if (OPTION_TIMIDITY)
        add_files(libtimidity.cpp)
    endif (OPTION_TIMIDITY)
    if (FLUIDSYNTH_FOUND)
        add_files(fluidsynth.cpp)
    endif (FLUIDSYNTH_FOUND)
endif (NOT OPTION_DEDICATED)

# OSX Files
if (APPLE)
    if (NOT OPTION_DEDICATED)
        add_files(qtmidi.cpp)
    endif (NOT OPTION_DEDICATED)
    if (OPTION_COCOA)
        add_files(cocoa_m.cpp)
    endif (OPTION_COCOA)
endif (APPLE)

# Header Files
if (ALLEGRO_FOUND)
    add_files(allegro_m.h)
    add_files(sound/allegro_s.h)
    add_files(video/allegro_v.h)
endif (ALLEGRO_FOUND)
add_files(dmusic.h)
add_files(
    midi.h
    midifile.hpp
    null_m.h
)
add_files(win32_m.h)
if (NOT WIN32)
    add_files(
        bemidi.h
        cocoa_m.h
        extmidi.h
        libtimidity.h
        fluidsynth.h
        os2_m.h
        qtmidi.h
    )
endif (NOT WIN32)

# Drivers
add_files(music_driver.hpp)
