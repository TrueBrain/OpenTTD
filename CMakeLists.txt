cmake_minimum_required(VERSION 3.5)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please run \"cmake ..\" from the bin directory")
endif (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

project(OpenTTD)

if (APPLE)
    set(AUTODETECT_OS "OSX")
elseif (WIN32)
    set(AUTODETECT_OS "WIN32")
elseif (MSVC)
    set(AUTODETECT_OS "WIN32")
elseif (UNIX)
    set(AUTODETECT_OS "UNIX")
endif ()

set(AUTODETECT_SDL NO)
set(AUTODETECT_COCOA NO)
set(AUTODETECT_DIRECTMUSIC NO)
set(AUTODETECT_UNISCRIBE NO)
set(AUTODETECT_GLOBAL_DIR "")
set(AUTODETECT_PERSONAL_DIR "")

if (AUTODETECT_OS STREQUAL "UNIX")
    set(AUTODETECT_SDL YES)
    set(AUTODETECT_GLOBAL_DIR "/share/games/openttd")
    set(AUTODETECT_PERSONAL_DIR ".openttd")
elseif (AUTODETECT_OS STREQUAL "WIN32")
    set(AUTODETECT_DIRECTMUSIC YES)
    set(AUTODETECT_UNISCRIBE YES)
    set(AUTODETECT_PERSONAL_DIR "OpenTTD")
elseif (AUTODETECT_OS STREQUAL "OSX")
    set(AUTODETECT_COCOA YES)
    set(AUTODETECT_PERSONAL_DIR "Documents/OpenTTD")
endif ()

set(OPTION_OS ${AUTODETECT_OS} CACHE STRING "OS to compile for")
set_property(CACHE OPTION_OS PROPERTY STRINGS UNIX WIN32)

set(OPTION_DEDICATED NO CACHE BOOL "Build dedicated server")
set(OPTION_NETWORK YES CACHE BOOL "Build with network support")

set(OPTION_ALLEGRO NO CACHE BOOL "Use Allegro")
set(OPTION_COCOA ${AUTODETECT_COCOA} CACHE BOOL "Use Cocoa")
set(OPTION_SDL ${AUTODETECT_SDL} CACHE BOOL "Use SDL")

set(OPTION_DIRECTMUSIC ${AUTODETECT_DIRECTMUSIC} CACHE BOOL "Enable DirectMusic")
set(OPTION_FLUIDSYNTH NO CACHE BOOL "Enable FluidSynth")
set(OPTION_LIBTIMIDITY NO CACHE BOOL "Enable LibTimidity")

set(OPTION_SSE NO CACHE BOOL "Use SSE support")
set(OPTION_HAVE_THREAD YES CACHE BOOL "Use threads")
set(OPTION_UNISCRIBE ${AUTODETECT_UNISCRIBE} CACHE BOOL "Use uniscribe")

set(OPTION_GLOBAL_DIR ${AUTODETECT_GLOBAL_DIR} CACHE STRING "Global data directory to use")
set(OPTION_PERSONAL_DIR ${AUTODETECT_PERSONAL_DIR} CACHE STRING "Personal data directory to use")


message(STATUS "OS: ${OPTION_OS}")
message(STATUS "Dedicated: ${OPTION_DEDICATED}")
message(STATUS "Network: ${OPTION_NETWORK}")
message(STATUS "Allegro: ${OPTION_ALLEGRO}")
message(STATUS "Cocoa: ${OPTION_COCOA}")
message(STATUS "SDL: ${OPTION_SDL}")
message(STATUS "DirectMusic: ${OPTION_DIRECTMUSIC}")
message(STATUS "FluidSynth: ${OPTION_FLUIDSYNTH}")
message(STATUS "LibTimiditiy: ${OPTION_LIBTIMIDITY}")
message(STATUS "SSE: ${OPTION_SSE}")
message(STATUS "Threads: ${OPTION_HAVE_THREAD}")
message(STATUS "Uniscribe: ${OPTION_UNISCRIBE}")
message(STATUS "Global dir: ${OPTION_GLOBAL_DIR}")
message(STATUS "Personal dir: ${OPTION_PERSONAL_DIR}")

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

    message(STATUS "Version string: ${REV_VERSION}")
else (FIND_VERSION_RESULT)
    message(WARNING "No version detected; this build will NOT be network compatible")
    set(REV_VERSION "norev0000")
    set(REV_ISODATE "19700101")
    set(REV_MODIFIED 1)
    set(REV_HASH "unknown")
    set(REV_ISTAG 0)
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

find_package(PNG)
if (PNG_FOUND)
    add_definitions(-DWITH_PNG)
    include_directories(${PNG_INCLUDE_DIR})
    target_link_libraries(openttd ${PNG_LIBRARIES})
else (PNG_FOUND)
    message(WARNING "libpng not found; functionality will be limited")
endif (PNG_FOUND)

find_package(ZLIB)
if (ZLIB_FOUND)
    add_definitions(-DWITH_ZLIB)
    include_directories(${ZLIB_INCLUDE_DIR})
    target_link_libraries(openttd ${ZLIB_LIBRARIES})
else (ZLIB_FOUND)
    message(WARNING "zlib not found; functionality will be limited")
endif (ZLIB_FOUND)

if (OPTION_SDL)
    add_definitions(-DWITH_SDL)
	find_package(SDL REQUIRED)
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

if (OPTION_SSE)
    add_definitions(-DWITH_SSE)
endif (OPTION_SSE)

if (OPTION_NETWORK)
    add_definitions(-DENABLE_NETWORK)
endif (OPTION_NETWORK)

if (OPTION_WIN32)
    add_definitions(-DWITH_ASSERT)  # TODO -- Should this be on in all situations?

    target_link_libraries(openttd
        ws2_32
        winmm
        imm32
    )
endif (OPTION_WIN32)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(-D_SQ64)
endif (CMAKE_SIZEOF_VOID_P EQUAL 8)
