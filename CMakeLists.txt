cmake_minimum_required(VERSION 3.5)

project(OpenTTD)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please run \"cmake ..\" from the bin directory")
endif (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

set(AUTODETECT_SDL NO)
set(AUTODETECT_COCOA NO)
set(AUTODETECT_DIRECTMUSIC NO)
set(AUTODETECT_UNISCRIBE NO)
set(AUTODETECT_GLOBAL_DIR "")
set(AUTODETECT_PERSONAL_DIR "")

if (APPLE)
    set(AUTODETECT_OS "OSX")
    set(AUTODETECT_COCOA YES)
    set(AUTODETECT_PERSONAL_DIR "Documents/OpenTTD")
elseif (WIN32 OR MSVC)
    set(AUTODETECT_OS "WIN32")
    set(AUTODETECT_DIRECTMUSIC YES)
    set(AUTODETECT_UNISCRIBE YES)
    set(AUTODETECT_PERSONAL_DIR "OpenTTD")
elseif (UNIX)
    set(AUTODETECT_OS "UNIX")
    set(AUTODETECT_GLOBAL_DIR "/share/games/openttd")
    set(AUTODETECT_PERSONAL_DIR ".openttd")
endif ()

find_package(ZLIB QUIET)
if (ZLIB_FOUND)
    set(AUTODETECT_ZLIB YES)
else (ZLIB_FOUND)
    set(AUTODETECT_ZLIB NO)
endif (ZLIB_FOUND)

find_package(PNG QUIET)
if (PNG_FOUND)
    set(AUTODETECT_PNG YES)
else (PNG_FOUND)
    set(AUTODETECT_PNG NO)
endif (PNG_FOUND)

find_package(SDL QUIET)
if (SDL_FOUND)
    set(AUTODETECT_SDL YES)
else (SDL_FOUND)
    set(AUTODETECT_SDL NO)
endif (SDL_FOUND)

include(FindPkgConfig)
pkg_search_module(FLUIDSYNTH fluidsynth)
if (FLUIDSYNTH_FOUND)
    set(AUTODETECT_FLUIDSYNTH YES)
else (FLUIDSYNTH_FOUND)
    set(AUTODETECT_FLUIDSYNTH NO)
endif (FLUIDSYNTH_FOUND)

include(CheckCXXSourceRuns)
set(CMAKE_REQUIRED_FLAGS "")
if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_REQUIRED_FLAGS "-msse4.1")
endif (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANG)

check_cxx_source_runs("
    #include <xmmintrin.h>
    #include <smmintrin.h>
    #include <tmmintrin.h>
    int main() { return 0; }"
    SSE_FOUND)
if (SSE_FOUND)
    set(AUTODETECT_SSE YES)
else (SSE_FOUND)
    set(AUTODETECT_SSE NO)
endif (SSE_FOUND)


set(OPTION_OS ${AUTODETECT_OS} CACHE STRING "OS to compile for")
set_property(CACHE OPTION_OS PROPERTY STRINGS OSX UNIX WIN32)
message(STATUS "OS: ${OPTION_OS}")

set(OPTION_DEDICATED NO CACHE BOOL "Build dedicated server only (no GUI)")
message(STATUS "Dedicated: ${OPTION_DEDICATED}")
set(OPTION_NETWORK YES CACHE BOOL "Build with network support")
message(STATUS "Network: ${OPTION_NETWORK}")

set(OPTION_ALLEGRO NO CACHE BOOL "Use Allegro drivers")
message(STATUS "Allegro: ${OPTION_ALLEGRO}")
set(OPTION_COCOA ${AUTODETECT_COCOA} CACHE BOOL "Use Cocoa drivers")
message(STATUS "Cocoa: ${OPTION_COCOA}")
set(OPTION_SDL ${AUTODETECT_SDL} CACHE BOOL "Use SDL drivers")
message(STATUS "SDL: ${OPTION_SDL}")

set(OPTION_DIRECTMUSIC ${AUTODETECT_DIRECTMUSIC} CACHE BOOL "Enable DirectMusic support")
message(STATUS "DirectMusic: ${OPTION_DIRECTMUSIC}")
set(OPTION_FLUIDSYNTH ${AUTODETECT_FLUIDSYNTH} CACHE BOOL "Enable FluidSynth support")
message(STATUS "FluidSynth: ${OPTION_FLUIDSYNTH}")
set(OPTION_LIBTIMIDITY NO CACHE BOOL "Enable LibTimidity support")
message(STATUS "LibTimiditiy: ${OPTION_LIBTIMIDITY}")

set(OPTION_ZLIB ${AUTODETECT_ZLIB} CACHE BOOL "Enable zlib support")
message(STATUS "zlib: ${OPTION_ZLIB}")
set(OPTION_PNG ${AUTODETECT_PNG} CACHE BOOL "Enable png support")
message(STATUS "png: ${OPTION_PNG}")
set(OPTION_UNISCRIBE ${AUTODETECT_UNISCRIBE} CACHE BOOL "Use uniscribe support")
message(STATUS "Uniscribe: ${OPTION_UNISCRIBE}")

set(OPTION_USE_SSE ${AUTODETECT_SSE} CACHE BOOL "Use SSE support")
message(STATUS "SSE: ${OPTION_USE_SSE}")
set(OPTION_USE_THREADS YES CACHE BOOL "Use threads")
message(STATUS "Threads: ${OPTION_USE_THREADS}")

set(OPTION_GLOBAL_DIR ${AUTODETECT_GLOBAL_DIR} CACHE STRING "Global data directory to use")
message(STATUS "Global dir: ${OPTION_GLOBAL_DIR}")
set(OPTION_PERSONAL_DIR ${AUTODETECT_PERSONAL_DIR} CACHE STRING "Personal data directory to use")
message(STATUS "Personal dir: ${OPTION_PERSONAL_DIR}")

if (OPTION_OS STREQUAL "UNIX")
    if (NOT OPTION_SDL)
        message(FATAL_ERROR "SDL is required for this platform")
    endif (NOT OPTION_SDL)
endif (OPTION_OS STREQUAL "UNIX")

if (OPTION_OS STREQUAL "OSX")
    if (NOT OPTION_COCOA)
        message(FATAL_ERROR "Cocoa is required for this platform")
    endif (NOT OPTION_COCOA)
endif (OPTION_OS STREQUAL "OSX")

if (OPTION_OS STREQUAL "WIN32")
endif (OPTION_OS STREQUAL "WIN32")

if (MSVC)
    # C++17 for MSVC
    set(CMAKE_CXX_STANDARD 17)
else (MSVC)
    # C++11 for all other targets
    set(CMAKE_CXX_STANDARD 11)
endif (MSVC)

set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

add_subdirectory(${CMAKE_SOURCE_DIR}/src/settingsgen)
add_subdirectory(${CMAKE_SOURCE_DIR}/src/strgen)

# source.list uses per OS a variable; translate the OS variable into a per variable.
set(OPTION_WIN32 NO)
set(OPTION_OSX NO)
set(OPTION_BEOS NO)
set(OPTION_DOS NO)
set(OPTION_MORPHOS NO)
set(OPTION_OS2 NO)
set(OPTION_${OPTION_OS} YES)

file(READ ${CMAKE_SOURCE_DIR}/source.list SOURCE_LIST)
string(REPLACE "\n" ";" SOURCE_LIST "${SOURCE_LIST}")
string(REGEX REPLACE ";\t*" ";" SOURCE_LIST "${SOURCE_LIST}")
string(REGEX REPLACE "[;]+;" ";" SOURCE_LIST "${SOURCE_LIST}")
string(REGEX REPLACE ";$" "" SOURCE_LIST "${SOURCE_LIST}")

set(SCAN_SKIP 0)
set(SCAN_DEPTH 0)
foreach (FILE IN LISTS SOURCE_LIST)
    if (${FILE} MATCHES "^#end")
        if (${SCAN_DEPTH} EQUAL ${SCAN_SKIP})
            math(EXPR SCAN_SKIP "${SCAN_SKIP} - 1")
        endif (${SCAN_DEPTH} EQUAL ${SCAN_SKIP})

        math(EXPR SCAN_DEPTH "${SCAN_DEPTH} - 1")

        continue()
    endif (${FILE} MATCHES "^#end")

    if (${FILE} MATCHES "^#else")
        if (${SCAN_DEPTH} EQUAL ${SCAN_SKIP})
            math(EXPR SCAN_SKIP "${SCAN_SKIP} - 1")
        else (${SCAN_DEPTH} EQUAL ${SCAN_SKIP})
            math(EXPR SCAN_DEPTH_PARENT "${SCAN_DEPTH} - 1")
            if (${SCAN_DEPTH_PARENT} EQUAL ${SCAN_SKIP})
                math(EXPR SCAN_SKIP "${SCAN_SKIP} + 1")
            endif (${SCAN_DEPTH_PARENT} EQUAL ${SCAN_SKIP})
        endif (${SCAN_DEPTH} EQUAL ${SCAN_SKIP})

        continue()
    endif (${FILE} MATCHES "^#else")

    if (${FILE} MATCHES "^#if")
        if (NOT ${SCAN_DEPTH} EQUAL ${SCAN_SKIP})
            math(EXPR SCAN_DEPTH "${SCAN_DEPTH} + 1")
            continue()
        endif (NOT ${SCAN_DEPTH} EQUAL ${SCAN_SKIP})

        math(EXPR SCAN_DEPTH "${SCAN_DEPTH} + 1")

        string(REGEX MATCH "#if (.*)" IF_DEFINE ${FILE})
        set(IF_DEFINE ${CMAKE_MATCH_1})

        if (DEFINED OPTION_${IF_DEFINE})
            if (${OPTION_${IF_DEFINE}})
                math(EXPR SCAN_SKIP "${SCAN_SKIP} + 1")
            endif (${OPTION_${IF_DEFINE}})
            continue()
        endif (DEFINED OPTION_${IF_DEFINE})

        message(FATAL_ERROR "Define ${IF_DEFINE} is not set")
    endif (${FILE} MATCHES "^#if")

    # Skip any other line starting with #
    if (${FILE} MATCHES "^#")
        continue()
    endif (${FILE} MATCHES "^#")

    # Skip the line if this is not our scan depth
    if (NOT ${SCAN_DEPTH} EQUAL ${SCAN_SKIP})
        continue()
    endif (NOT ${SCAN_DEPTH} EQUAL ${SCAN_SKIP})

    # TODO -- For now, ignore these ugly entries
    if (${FILE} MATCHES "../objs/*")
        continue()
    endif (${FILE} MATCHES "../objs/*")

    list(APPEND SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/${FILE})
endforeach(FILE)

# TODO -- Don't put this in here like this; make this a target or something
# TODO -- Run this every run
execute_process(COMMAND ./findversion.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE FIND_VERSION_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (FIND_VERSION_RESULT)
    string(REPLACE "\t" ";" FIND_VERSION ${FIND_VERSION_RESULT})
    list(GET FIND_VERSION 0 REV_VERSION)
    list(GET FIND_VERSION 1 REV_ISODATE)
    list(GET FIND_VERSION 2 REV_MODIFIED)
    list(GET FIND_VERSION 3 REV_HASH)
    list(GET FIND_VERSION 4 REV_ISTAG)
    list(GET FIND_VERSION 5 REV_ISSTABLETAG)

    message(STATUS "Version string: ${REV_VERSION}")
else (FIND_VERSION_RESULT)
    message(WARNING "No version detected; this build will NOT be network compatible")
    set(REV_VERSION "norev0000")
    set(REV_ISODATE "19700101")
    set(REV_MODIFIED 1)
    set(REV_HASH "unknown")
    set(REV_ISTAG 0)
    set(REV_ISSTABLETAG 0)
endif (FIND_VERSION_RESULT)

message(STATUS "Generating src/rev.cpp")
configure_file("${CMAKE_SOURCE_DIR}/src/rev.cpp.in"
        "${CMAKE_BINARY_DIR}/generated/rev.cpp")
set_source_files_properties("${CMAKE_BINARY_DIR}/generated/rev.cpp" GENERATED)
list(APPEND SOURCE_FILES "${CMAKE_BINARY_DIR}/generated/rev.cpp")

if (OPTION_WIN32)
    message(STATUS "Generating src/os/windows/ottdres.rc")
    configure_file("${CMAKE_SOURCE_DIR}/src/os/windows/ottdres.rc.in"
            "${CMAKE_BINARY_DIR}/generated/ottdres.rc")
    set_source_files_properties("${CMAKE_BINARY_DIR}/generated/ottdres.rc" GENERATED)
    list(APPEND SOURCE_FILES "${CMAKE_BINARY_DIR}/generated/ottdres.rc")
endif (OPTION_WIN32)

include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if (IS_BIG_ENDIAN)
    add_definitions(-DTTD_ENDIAN=TTD_BIG_ENDIAN)
else (IS_BIG_ENDIAN)
    add_definitions(-DTTD_ENDIAN=TTD_LITTLE_ENDIAN)
endif (IS_BIG_ENDIAN)

if (MSVC)
    # Build OpenTTD in a static form via MSVC
    set(CMAKE_CXX_FLAGS_RELEASE "/MT")
    set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
else (MSVC)
    add_compile_options(-Wno-multichar)
endif (MSVC)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/3rdparty/squirrel/include)

add_executable(openttd
        WIN32
        ${SOURCE_FILES})

target_link_libraries(openttd
    openttd::languages
    openttd::settings
)

set_target_properties(openttd PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

add_definitions(-D${OPTION_OS})

if (NOT OPTION_PNG)
    message(WARNING "png not selected; compiling OpenTTD without png is strongly disencouraged")
elseif (NOT PNG_FOUND)
    message(FATAL_ERROR "png not found but was expected to be found")
endif ()

if (NOT OPTION_ZLIB)
    message(WARNING "zlib not selected; compiling OpenTTD without zlib is strongly disencouraged")
elseif (NOT ZLIB_FOUND)
    message(FATAL_ERROR "zlib not found but was expected to be found")
endif ()

if (NOT OPTION_FLUIDSYNTH)
elseif (NOT FLUIDSYNTH_FOUND)
    message(FATAL_ERROR "fluidsync not found but was expected to be found")
endif ()

if (NOT OPTION_SDL)
elseif (NOT SDL_FOUND)
    message(FATAL_ERROR "SDL not found but was expected to be found")
endif ()

if (NOT OPTION_USE_SSE)
elseif (NOT SSE_FOUND)
    message(FATAL_ERROR "SSE not found but was expected to be found")
endif ()

if (OPTION_PNG)
    add_definitions(-DWITH_PNG)
    include_directories(${PNG_INCLUDE_DIR})
    target_link_libraries(openttd ${PNG_LIBRARIES})
endif (OPTION_PNG)

if (OPTION_ZLIB)
    add_definitions(-DWITH_ZLIB)
    include_directories(${ZLIB_INCLUDE_DIR})
    target_link_libraries(openttd ${ZLIB_LIBRARIES})
endif (OPTION_ZLIB)

if (OPTION_FLUIDSYNTH)
    include_directories(${FLUIDSYNTH_INCLUDE_DIRS})
    target_link_libraries(openttd ${FLUIDSYNTH_LIBRARIES})
endif (OPTION_FLUIDSYNTH)

if (OPTION_SDL)
    add_definitions(-DWITH_SDL)
	include_directories(${SDL_INCLUDE_DIR})
	target_link_libraries(openttd ${SDL_LIBRARY})
endif (OPTION_SDL)

if (OPTION_GLOBAL_DIR)
    add_definitions(-DGLOBAL_DATA_DIR="${OPTION_GLOBAL_DIR}")
endif (OPTION_GLOBAL_DIR)

if (OPTION_PERSONAL_DIR)
    add_definitions(-DWITH_PERSONAL_DIR)
    add_definitions(-DPERSONAL_DIR="${OPTION_PERSONAL_DIR}")
endif (OPTION_PERSONAL_DIR)

if (OPTION_UNISCRIBE)
    add_definitions(-DWITH_UNISCRIBE)
endif (OPTION_UNISCRIBE)

if (OPTION_USE_SSE)
    add_definitions(-DWITH_SSE)

    # Add the correct (S)SSE flag for (S)SSE files
    file(GLOB SSE2_FILES ${CMAKE_SOURCE_DIR}/src/*_sse2.cpp ${CMAKE_SOURCE_DIR}/src/*/*_sse2.cpp)
    file(GLOB SSSE3_FILES ${CMAKE_SOURCE_DIR}/src/*_ssse3.cpp ${CMAKE_SOURCE_DIR}/src/*/*_ssse3.cpp)
    file(GLOB SSE4_FILES ${CMAKE_SOURCE_DIR}/src/*_sse4.cpp ${CMAKE_SOURCE_DIR}/src/*/*_sse4.cpp)
    set_source_files_properties(${SSE2_FILES} PROPERTIES COMPILE_FLAGS -msse2)
    set_source_files_properties(${SSSE3_FILES} PROPERTIES COMPILE_FLAGS -mssse3)
    set_source_files_properties(${SSE4_FILES} PROPERTIES COMPILE_FLAGS -msse4.1)
endif (OPTION_USE_SSE)

if (OPTION_NETWORK)
    add_definitions(-DENABLE_NETWORK)
endif (OPTION_NETWORK)

if (NOT OPTION_USE_THREADS)
    add_definitions(-DNO_THREADS)
endif (NOT OPTION_USE_THREADS)

if (OPTION_WIN32)
    add_definitions(-DWITH_ASSERT)  # TODO -- Should this be on in all situations?
    add_definitions(-DUNICODE -D_UNICODE)

    target_link_libraries(openttd
        ws2_32
        winmm
        imm32
    )
endif (OPTION_WIN32)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(-D_SQ64)
endif (CMAKE_SIZEOF_VOID_P EQUAL 8)
