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

        # pkg-config returns the libraries without really resolving them.
        # This has as huge drawback that sometimes you only notice on linking
        # a library is missing. To do better detecting during configuration,
        # run all the libraries pkg-config returns through detection again, to
        # see if they really exist.
        # This is also how most CMake files work, and fixes issues on OSX where
        # libraries are otherwise not always detected (because CMake usees more
        # library paths than the compiler normally does).
        if (${NAME}_FOUND)
            unset(PKGCONFIG_LIBRARIES)
            foreach(PKGCONFIG_LIBRARY ${${NAME}_LIBRARIES})
                unset(FIND_PKGCONFIG_LIBRARY CACHE)
                find_library(FIND_PKGCONFIG_LIBRARY NAMES "${PKGCONFIG_LIBRARY}")

                if (FIND_PKGCONFIG_LIBRARY STREQUAL "FIND_PKGCONFIG_LIBRARY-NOTFOUND")
                    message(FATAL_ERROR "pkg-config indicates '${PKGCONFIG}' depends on library '${PKGCONFIG_LIBRARY}' but this library was not found on the system")
                endif (FIND_PKGCONFIG_LIBRARY STREQUAL "FIND_PKGCONFIG_LIBRARY-NOTFOUND")

                list(APPEND PKGCONFIG_LIBRARIES ${FIND_PKGCONFIG_LIBRARY})
            endforeach()
            unset(FIND_PKGCONFIG_LIBRARY CACHE)

            set(${NAME}_LIBRARIES "${PKGCONFIG_LIBRARIES}")
        endif (${NAME}_FOUND)
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

macro(_patch_older_cmake NAME)
    # Patch up libraries that still announce under the deprecated name only
    if (NOT ${NAME}_LIBRARIES)
        set(${NAME}_LIBRARIES "${${NAME}_LIBRARY}")
    endif (NOT ${NAME}_LIBRARIES)
    if (NOT ${NAME}_INCLUDE_DIRS)
        set(${NAME}_INCLUDE_DIRS "${${NAME}_INCLUDE_DIR}")
    endif (NOT ${NAME}_INCLUDE_DIRS)
endmacro()

macro(_patch_vcpkg_debug_optimized NAME)
    # With vcpkg, the library path should contain both 'debug' and 'optimized'
    # entries (see target_link_libraries() documentation for more information)
    #
    # Some libraries do this, but not all. In cases where this goes wrong,
    # we will fix things up ourself.
    #
    # NOTE: we only patch up when using vcpkg; the same issue might happen
    # when not using vcpkg, but this is non-trivial to fix, as we have no idea
    # what the paths are. With vcpkg we do. And we only official support vcpkg
    # with Windows.
    #
    # NOTE: this is based on the assumption that the debug file has the same
    # name as the optimized file. This is not always the case, but so far
    # experiences has shown that in those case vcpkg CMake files do the right
    # thing.

    if (VCPKG_TOOLCHAIN AND ${NAME}_LIBRARIES)
        # Is this entry already of the correct format?
        string(FIND "${${NAME}_LIBRARIES}" "optimized;" HAS_OPTIMIZED)
        unset(PATCH_LIBRARIES)

        if (HAS_OPTIMIZED LESS 0)
            foreach(PATCH_LIBRARY ${${NAME}_LIBRARIES})
                # Check if we have a 'debug' entry or an 'optimized' entry, and
                # create the complementary entry.
                if (${PATCH_LIBRARY} MATCHES "/debug/")
                    set(PATCH_LIBRARY_DEBUG "${PATCH_LIBRARY}")
                    string(REPLACE "/debug/lib/" "/lib/" PATCH_LIBRARY_RELEASE "${PATCH_LIBRARY}")
                else (${LIBRARY} MATCHES "/debug/")
                    set(PATCH_LIBRARY_RELEASE "${PATCH_LIBRARY}")
                    string(REPLACE "/lib/" "/debug/lib/" PATCH_LIBRARY_DEBUG "${PATCH_LIBRARY}")
                endif (${PATCH_LIBRARY} MATCHES "/debug/")

                LIST(APPEND PATCH_LIBRARIES
                    "optimized" "${PATCH_LIBRARY_RELEASE}"
                    "debug" "${PATCH_LIBRARY_DEBUG}")
            endforeach()

            # Reassembly a single variable that sets both
            set(${NAME}_LIBRARIES ${PATCH_LIBRARIES})
        endif (HAS_OPTIMIZED LESS 0)
    endif (VCPKG_TOOLCHAIN AND ${NAME}_LIBRARIES)
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
        _patch_older_cmake("${NAME}")
        _patch_vcpkg_debug_optimized("${NAME}")

        message(STATUS "${NAME}_INCLUDE_DIRS: ${${NAME}_INCLUDE_DIRS}")  # TODO: temporary debug
        message(STATUS "${NAME}_LIBRARIES: ${${NAME}_LIBRARIES}") # TODO: temporary debug

        message(STATUS "Detecting ${FRIENDLY} - found")
    else (${NAME}_FOUND)
        message(STATUS "Detecting ${FRIENDLY} - not found")
    endif (${NAME}_FOUND)
endmacro()

macro(link_library NAME)
    set(args ${ARGN})

    # Check if any of the argument is ENCOURAGED
    list(FIND args ENCOURAGED ENCOURAGED)
    if (ENCOURAGED LESS 0)
        set(ENCOURAGED NO)
    else (ENCOURAGED LESS 0)
        set(ENCOURAGED YES)
    endif (ENCOURAGED LESS 0)

    if (${NAME}_FOUND)
        add_definitions(-DWITH_${NAME})
        include_directories(${${NAME}_INCLUDE_DIRS})
        target_link_libraries(openttd ${${NAME}_LIBRARIES})
    else (${NAME}_FOUND)
        if (ENCOURAGED)
            message(WARNING "${NAME} not found; compiling OpenTTD without ${NAME} is strongly disencouraged")
        endif (ENCOURAGED)
    endif (${NAME}_FOUND)
endmacro()
