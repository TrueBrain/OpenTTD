cmake_minimum_required(VERSION 3.5)

if (APPLE)
    add_files(crashlog_osx.cpp)
endif (APPLE)
if (NOT WIN32)
    add_files(
        macos.h
        osx_stdafx.h
        splash.h
        string_osx.h
    )
endif (NOT WIN32)
if (APPLE)
    add_files(macos.mm)
    if (OPTION_COCOA)
        add_files(
            splash.cpp
            string_osx.cpp
        )
	endif (OPTION_COCOA)
endif (APPLE)
