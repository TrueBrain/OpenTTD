cmake_minimum_required(VERSION 3.5)

# Windows files
if (WIN32)
    add_files(
        crashlog_win.cpp
        string_uniscribe.cpp
        win32.cpp
    )
endif (WIN32)

# Header Files
add_files(string_uniscribe.h)
add_files(win32.h)
