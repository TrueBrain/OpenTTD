#
# Try to autodetect a library via various of means.
#
# We first assume that find_package() can find it.
# Next we try it via pkg-config.
# If all that fails, do a find_path()/find_library(), as last effort.
#
# This is done as we support a wide variaty of systems, and not all have the
# CMake files we need. For example, lzo doesn't even have pkg-config files
# in Debian Stretch (but does in Debian Sid).
#

macro(_autodetect_library_via_pkgconfig NAME PKGCONFIG)
    include(FindPkgConfig QUIET)

    if (PKG_CONFIG_FOUND)
        pkg_search_module("${NAME}" "${PKGCONFIG}" QUIET)
    endif (PKG_CONFIG_FOUND)
endmacro()

macro(_autodetect_library_via_find NAME HEADER_PATH HEADER LIBRARY)
    find_path(${NAME}_INCLUDE_DIR NAMES "${HEADER}" PATH_SUFFIXES "${HEADER_PATH}")
    find_library(${NAME}_LIBRARY NAMES "${LIBRARY}")

    if (${NAME}_INCLUDE_DIR AND ${NAME}_LIBRARY)
        set(${NAME}_FOUND YES)
        set(${NAME}_INCLUDE_DIRS ${${NAME}_INCLUDE_DIR})
        set(${NAME}_INCLUDE_LIBRARIES ${${NAME}_LIBRARY})
    endif (${NAME}_INCLUDE_DIR AND ${NAME}_LIBRARY)
endmacro()

macro(autodetect_library FRIENDLY NAME PACKAGE PKGCONFIG HEADER_PATH HEADER LIBRARY)
    message(STATUS "Detecting ${FRIENDLY}")

    find_package("${PACKAGE}" QUIET)

    if (NOT ${NAME}_FOUND)
        message(STATUS "Trying with pkgconfig") # TODO: temporary debug
        _autodetect_library_via_pkgconfig("${NAME}" "${PKGCONFIG}")
    endif (NOT ${NAME}_FOUND)
    if (NOT ${NAME}_FOUND)
        message(STATUS "Trying with find") # TODO: temporary debug
        _autodetect_library_via_find("${NAME}" "${HEADER_PATH}" "${HEADER}" "${LIBRARY}")
    endif (NOT ${NAME}_FOUND)

    if (${NAME}_FOUND)
        # Patch up libraries that still announce under the deprecated name only
        if (NOT ${NAME}_LIBRARIES)
            set(${NAME}_LIBRARIES "${${NAME}_LIBRARY}")
        endif (NOT ${NAME}_LIBRARIES)
        if (NOT ${NAME}_INCLUDE_DIRS)
            set(${NAME}_INCLUDE_DIRS "${${NAME}_INCLUDE_DIR}")
        endif (NOT ${NAME}_INCLUDE_DIRS)

        message(STATUS "${NAME}_INCLUDE_DIRS: ${${NAME}_INCLUDE_DIRS}")  # TODO: temporary debug
        message(STATUS "${NAME}_LIBRARIES: ${${NAME}_LIBRARIES}") # TODO: temporary debug

        message(STATUS "Detecting ${FRIENDLY} - found")
    else (${NAME}_FOUND)
        message(STATUS "Detecting ${FRIENDLY} - not found")
    endif (${NAME}_FOUND)
endmacro()
