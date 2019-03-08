cmake_minimum_required(VERSION 3.5)

add_subdirectory(md5)
add_subdirectory(squirrel)
if (OPTION_OS2)
    add_subdirectory(os2)
endif (OPTION_OS2)
