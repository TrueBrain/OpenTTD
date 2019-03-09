cmake_minimum_required(VERSION 3.5)

if (NOT WIN32)
    add_files(
        cocoa/cocoa_keys.h
        cocoa/cocoa_v.h
    )
endif (NOT WIN32)

if (APPLE)
# OSX Files
    if (OPTION_COCOA)
        add_files(
            cocoa/cocoa_v.mm
            cocoa/event.mm
            cocoa/fullscreen.mm
            cocoa/wnd_quartz.mm
            cocoa/wnd_quickdraw.mm
        )
    endif (OPTION_COCOA)
endif (APPLE)
