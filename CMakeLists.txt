cmake_minimum_required(VERSION 3.5)

project(OpenTTD)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please run \"cmake ..\" from the bin directory")
endif (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

set(OPTION_OS "")
set(OPTION_COCOA NO)
set(OPTION_DIRECTMUSIC NO)
set(OPTION_UNISCRIBE NO)
set(OPTION_GLOBAL_DIR "")
set(OPTION_PERSONAL_DIR "")

if (APPLE)
    set(OPTION_OS "OSX")
    set(OPTION_COCOA YES)
    set(OPTION_PERSONAL_DIR "Documents/OpenTTD")
elseif (WIN32 OR MSVC)
    set(OPTION_OS "WIN32")
    set(OPTION_DIRECTMUSIC YES)
    set(OPTION_UNISCRIBE YES)
    set(OPTION_PERSONAL_DIR "OpenTTD")
elseif (UNIX)
    set(OPTION_OS "UNIX")
    set(OPTION_GLOBAL_DIR "/share/games/openttd")
    set(OPTION_PERSONAL_DIR ".openttd")
endif ()

message(STATUS "Detecting OS - ${OPTION_OS}")
message(STATUS "Detecting Cocoa - ${OPTION_COCOA}")
message(STATUS "Detecting DirectMusic - ${OPTION_DIRECTMUSIC}")
message(STATUS "Detecting Uniscribe - ${OPTION_UNISCRIBE}")
message(STATUS "Detecting Global Data directory - ${OPTION_GLOBAL_DIR}")
message(STATUS "Detecting Personal Data directory - ${OPTION_PERSONAL_DIR}")

include(AutodetectLibrary REQUIRED)
autodetect_library("zlib" ZLIB ZLIB zlib zlib.h libz)
autodetect_library("LZMA" LIBLZMA LibLZMA liblzma lzma.h liblzma)
autodetect_library("lzo" LZO LZO lzo2 lzo/lzo1x.h lzo2)
autodetect_library("PNG" PNG PNG libpng png.h libpng)
autodetect_library("SDL" SDL SDL sdl SDL/SDL.h libSDL)
autodetect_library("fluidsynth" FLUIDSYNTH FLUIDSYNTH fluidsynth fluidsynth.h fluidsynth)

include(AutodetectSSE REQUIRED)
autodetect_sse()

set(OPTION_DEDICATED NO CACHE BOOL "Build dedicated server only (no GUI)")
message(STATUS "Option Dedicated - ${OPTION_DEDICATED}")
set(OPTION_NETWORK YES CACHE BOOL "Build with network support")
message(STATUS "Option Network - ${OPTION_NETWORK}")
set(OPTION_USE_THREADS YES CACHE BOOL "Use threads")
message(STATUS "Option Threads - ${OPTION_USE_THREADS}")
set(OPTION_USE_ASSERTS YES CACHE STRING "Use assertions; leave enabled for nightlies, betas, and RCs")
message(STATUS "Option Assert - ${OPTION_USE_ASSERTS}")

set(OPTION_ALLEGRO NO CACHE BOOL "Use Allegro drivers")
set(OPTION_TIMIDITY NO CACHE BOOL "Enable LibTimidity support")

if (OPTION_OS STREQUAL "UNIX")
    if (NOT SDL_FOUND)
        message(FATAL_ERROR "SDL is required for this platform")
    endif (NOT SDL_FOUND)
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

include(SourceList REQUIRED)
source_list()

# TODO -- Don't put this in here like this; make this a target or something
# TODO -- Run this every run
include(FindVersion REQUIRED)
find_version()

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

include(Endian REQUIRED)
add_endian_definition()

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

if (NOT PNG_FOUND)
    message(WARNING "PNG not found; compiling OpenTTD without PNG is strongly disencouraged")
endif ()

if (NOT ZLIB_FOUND)
    message(WARNING "zlib not found; compiling OpenTTD without zlib is strongly disencouraged")
endif ()

if (NOT LIBLZMA_FOUND)
    message(WARNING "LZMA not found; compiling OpenTTD without LZMA is strongly disencouraged")
endif ()

if (NOT LZO_FOUND)
    message(WARNING "LZO not found; compiling OpenTTD without LZO is strongly disencouraged")
endif ()

if (PNG_FOUND)
    add_definitions(-DWITH_PNG)
    include_directories(${PNG_INCLUDE_DIRS})
    target_link_libraries(openttd ${PNG_LIBRARIES})
endif (PNG_FOUND)

if (ZLIB_FOUND)
    add_definitions(-DWITH_ZLIB)
    include_directories(${ZLIB_INCLUDE_DIRS})
    target_link_libraries(openttd ${ZLIB_LIBRARIES})
endif (ZLIB_FOUND)

if (LIBLZMA_FOUND)
    add_definitions(-DWITH_LZMA)
    include_directories(${LIBLZMA_INCLUDE_DIRS})
    target_link_libraries(openttd ${LIBLZMA_LIBRARIES})
endif (LIBLZMA_FOUND)

if (LZO_FOUND)
    add_definitions(-DWITH_LZO)
    include_directories(${LZO_INCLUDE_DIRS})
    target_link_libraries(openttd ${LZO_LIBRARIES})
endif (LZO_FOUND)

if (FLUIDSYNTH_FOUND)
    include_directories(${FLUIDSYNTH_INCLUDE_DIRS})
    target_link_libraries(openttd ${FLUIDSYNTH_LIBRARIES})
endif (FLUIDSYNTH_FOUND)

if (SDL_FOUND)
    add_definitions(-DWITH_SDL)
	include_directories(${SDL_INCLUDE_DIR})
	target_link_libraries(openttd ${SDL_LIBRARY})
endif (SDL_FOUND)

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

if (OPTION_USE_ASSERTS)
    add_definitions(-DWITH_ASSERT)
else (OPTION_USE_ASSERTS)
    add_definitions(-DNDEBUG)
endif (OPTION_USE_ASSERTS)

if (OPTION_WIN32)
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
