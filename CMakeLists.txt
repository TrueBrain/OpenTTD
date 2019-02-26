cmake_minimum_required(VERSION 3.5)

project(OpenTTD)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please run \"cmake ..\" from the bin directory")
endif (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

if (APPLE)
    set(PERSONAL_DIR "Documents/OpenTTD")
    set(SHARED_DIR "/Library/Application Support/OpenTTD")
    set(GLOBAL_DIR "")
elseif (WIN32)
    set(PERSONAL_DIR "OpenTTD")
    set(SHARED_DIR "")
    set(GLOBAL_DIR "")
elseif (UNIX)
    set(PERSONAL_DIR ".openttd")
    set(SHARED_DIR "")
    set(GLOBAL_DIR "${CMAKE_INSTALL_PREFIX}/share/games/openttd")
else ()
    message(FATAL_ERROR "Unknown OS found; please consider creating a Pull Request to add support for this OS.")
endif ()

if (GLOBAL_DIR)
    message(STATUS "Detecting Global Data directory - ${GLOBAL_DIR}")
else (GLOBAL_DIR)
    message(STATUS "Detecting Global Data directory - (not set)")
endif (GLOBAL_DIR)

if (PERSONAL_DIR)
    message(STATUS "Detecting Personal Data directory - ${PERSONAL_DIR}")
else (PERSONAL_DIR)
    message(STATUS "Detecting Personal Data directory - (not set)")
endif (PERSONAL_DIR)

if (SHARED_DIR)
    message(STATUS "Detecting Shared Data directory - ${SHARED_DIR}")
else (SHARED_DIR)
    message(STATUS "Detecting Shared Data directory - (not set)")
endif (SHARED_DIR)

include(AutodetectLibrary REQUIRED)
autodetect_library("zlib" ZLIB ZLIB zlib zlib.h libz)
autodetect_library("LZMA" LIBLZMA LibLZMA liblzma lzma.h liblzma)
autodetect_library("lzo" LZO LZO lzo2 lzo/lzo1x.h lzo2)
autodetect_library("PNG" PNG PNG libpng png.h libpng)
autodetect_library("SDL" SDL SDL sdl SDL/SDL.h libSDL)
autodetect_library("Allegro" ALLEGRO ALLEGRO allegro allegro.h alleg-dga2)
autodetect_library("fluidsynth" FLUIDSYNTH FLUIDSYNTH fluidsynth fluidsynth.h fluidsynth)
autodetect_library("FreeType" FREETYPE Freetype freetype2 freetype2/ft2build.h freetype)
autodetect_library("FontConfig" FONTCONFIG FONTCONFIG fontconfig fontconfig/fontconfig.h fontconfig)

include(AutodetectSSE REQUIRED)
autodetect_sse()

include(AutodetectXaudio2 REQUIRED)
autodetect_xaudio2()

set(OPTION_DEDICATED NO CACHE BOOL "Build dedicated server only (no GUI)")
message(STATUS "Option Dedicated - ${OPTION_DEDICATED}")
set(OPTION_NETWORK YES CACHE BOOL "Build with network support")
message(STATUS "Option Network - ${OPTION_NETWORK}")
set(OPTION_USE_THREADS YES CACHE BOOL "Use threads")
message(STATUS "Option Threads - ${OPTION_USE_THREADS}")
set(OPTION_USE_ASSERTS YES CACHE STRING "Use assertions; leave enabled for nightlies, betas, and RCs")
message(STATUS "Option Assert - ${OPTION_USE_ASSERTS}")

# TODO -- Support timidity
set(OPTION_TIMIDITY NO)

if (UNIX)
    if (NOT SDL_FOUND)
        message(FATAL_ERROR "SDL is required for this platform")
    endif (NOT SDL_FOUND)
endif (UNIX)

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

if (WIN32)
    message(STATUS "Generating src/os/windows/ottdres.rc")
    configure_file("${CMAKE_SOURCE_DIR}/src/os/windows/ottdres.rc.in"
            "${CMAKE_BINARY_DIR}/generated/ottdres.rc")
    set_source_files_properties("${CMAKE_BINARY_DIR}/generated/ottdres.rc" GENERATED)
    list(APPEND SOURCE_FILES "${CMAKE_BINARY_DIR}/generated/ottdres.rc")
endif (WIN32)

include(Endian REQUIRED)
add_endian_definition()

if (MSVC)
    # Build OpenTTD in a static form via MSVC
    set(CMAKE_CXX_FLAGS_RELEASE "/MT")
    set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
    list(APPEND SOURCE_FILES "${CMAKE_SOURCE_DIR}/projects/openttd.manifest")
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

if (APPLE OR UNIX)
    add_definitions(-DUNIX)
endif (APPLE OR UNIX)

include(OpenTTD REQUIRED)

add_library(PNG ENCOURAGED)
add_library(ZLIB ENCOURAGED)
add_library(LIBLZMA ENCOURAGED)
add_library(LZO ENCOURAGED)
add_library(FLUIDSYNTH)
add_library(SDL)
add_library(ALLEGRO)
add_library(FREETYPE)
add_library(FONTCONFIG)

if (GLOBAL_DIR)
    add_definitions(-DGLOBAL_DATA_DIR="${GLOBAL_DIR}")
endif (GLOBAL_DIR)

if (PERSONAL_DIR)
    add_definitions(-DWITH_PERSONAL_DIR)
    add_definitions(-DPERSONAL_DIR="${PERSONAL_DIR}")
endif (PERSONAL_DIR)

if (SHARED_DIR)
    add_definitions(-DWITH_SHARED_DIR)
    add_definitions(-DSHARED_DIR="${SHARED_DIR}")
endif (SHARED_DIR)

if (SSE_FOUND)
    add_definitions(-DWITH_SSE)

    # Add the correct (S)SSE flag for (S)SSE files
    file(GLOB SSE2_FILES ${CMAKE_SOURCE_DIR}/src/*_sse2.cpp ${CMAKE_SOURCE_DIR}/src/*/*_sse2.cpp)
    file(GLOB SSSE3_FILES ${CMAKE_SOURCE_DIR}/src/*_ssse3.cpp ${CMAKE_SOURCE_DIR}/src/*/*_ssse3.cpp)
    file(GLOB SSE4_FILES ${CMAKE_SOURCE_DIR}/src/*_sse4.cpp ${CMAKE_SOURCE_DIR}/src/*/*_sse4.cpp)
    set_source_files_properties(${SSE2_FILES} PROPERTIES COMPILE_FLAGS -msse2)
    set_source_files_properties(${SSSE3_FILES} PROPERTIES COMPILE_FLAGS -mssse3)
    set_source_files_properties(${SSE4_FILES} PROPERTIES COMPILE_FLAGS -msse4.1)
endif (SSE_FOUND)

if (OPTION_NETWORK)
    add_definitions(-DENABLE_NETWORK)
endif (OPTION_NETWORK)

if (OPTION_DEDICATED)
    add_definitions(-DDEDICATED)
endif (OPTION_DEDICATED)

if (NOT OPTION_USE_THREADS)
    add_definitions(-DNO_THREADS)
endif (NOT OPTION_USE_THREADS)

if (OPTION_USE_ASSERTS)
    add_definitions(-DWITH_ASSERT)
else (OPTION_USE_ASSERTS)
    add_definitions(-DNDEBUG)
endif (OPTION_USE_ASSERTS)

if (WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-DWITH_UNISCRIBE)

    target_link_libraries(openttd
        ws2_32
        winmm
        imm32
    )
endif (WIN32)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(-D_SQ64)
endif (CMAKE_SIZEOF_VOID_P EQUAL 8)
