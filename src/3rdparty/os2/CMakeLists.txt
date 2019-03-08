cmake_minimum_required(VERSION 3.5)

if (OPTION_OS2)
    add_files(
        getaddrinfo.c
        getaddrinfo.h
        getnameinfo.c
        getnameinfo.h
    )
endif (OPTION_OS2)
