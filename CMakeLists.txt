cmake_minimum_required(VERSION 3.5)

project(OpenTTD)

if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please run \"cmake ..\" from the bin directory")
endif (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

include(Options REQUIRED)
set_options()

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

# Make sure we have Threads available. We don't use autodetect_library here,
# as the CMake 'FindThreads' is a lot more clever, always available, and
# gives us a Threads::Threads dependency to link to our application.
find_package(Threads REQUIRED)

include(AutodetectLibrary REQUIRED)
autodetect_library("zlib" ZLIB ZLIB zlib "" zlib.h z)
autodetect_library("LZMA" LIBLZMA LibLZMA liblzma "" lzma.h lzma)
autodetect_library("lzo" LZO LZO lzo2 lzo lzo1x.h lzo2 lzo)
autodetect_library("PNG" PNG PNG libpng "" png.h png)
autodetect_library("SDL" SDL SDL sdl SDL SDL.h SDL)
autodetect_library("Allegro" ALLEGRO ALLEGRO allegro "" allegro.h alleg)
autodetect_library("fluidsynth" FLUIDSYNTH FLUIDSYNTH fluidsynth "" fluidsynth.h fluidsynth)
autodetect_library("FreeType" FREETYPE Freetype freetype2 freetype2 ft2build.h freetype)
autodetect_library("FontConfig" FONTCONFIG FONTCONFIG fontconfig fontconfig fontconfig.h fontconfig)
autodetect_library("icu-lx" ICU_lx ICU icu-lx "" NOTFOUND NOTFOUND) # Manual lookup is not supported
autodetect_library("icu-i18n" ICU_i18n ICU icu-i18n "" NOTFOUND NOTFOUND) # Manual lookup is not supported
autodetect_library("xdg-basedir" XDG_BASEDIR XDG_BASEDIR libxdg-basedir "" basedir.h xdg-basedir)
if (APPLE)
    autodetect_library("iconv" Iconv Iconv iconv "" iconv.h iconv)

    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
    find_library(AUDIOUNIT_LIBRARY AudioUnit)
    find_library(CARBON_LIBRARY Carbon)
    find_library(COCOA_LIBRARY Cocoa)
endif (APPLE)

# TODO -- Support timidity
set(OPTION_TIMIDITY NO)

# LZO is an utter mess, and works on most platforms by accident.
# The pkg-config that ships with LZO, sets the includepath to the folder
# "lzo", so something like "/usr/include/lzo".
# Now the file "lzo/lzo1x.h" includes "lzo/lzoconf.h". This obvious is wrong,
# as there is no way to include that based on the includepath. Now this, by
# accident, goes unnoticed for most platforms, as it is very likely
# "/usr/include" is also on your searchpath. Other platforms have fixed this
# issue by patching "lzo/lzo1x.h" to include "lzoconf.h" (which is honestly
# the only correct way to do this). With brew on OSX however, they don't
# modify the source, use an unpatched pkg-config, and don't put the root
# folder in the search path for includes. So there this breaks.
# As such, we 'fix' things here by also making sure the root folder is
# included in the search path.
if (LZO_FOUND)
    get_filename_component(LZO_INCLUDE_DIRS "${LZO_INCLUDE_DIRS}/.." ABSOLUTE)
endif (LZO_FOUND)

include(AutodetectSSE REQUIRED)
autodetect_sse()

include(AutodetectXaudio2 REQUIRED)
autodetect_xaudio2()

show_options()

if (UNIX AND NOT APPLE AND NOT OPTION_DEDICATED)
    if (NOT SDL_FOUND)
        message(FATAL_ERROR "SDL is required for this platform")
    endif (NOT SDL_FOUND)
endif (UNIX AND NOT APPLE AND NOT OPTION_DEDICATED)
if (APPLE)
    if (NOT AUDIOTOOLBOX_LIBRARY)
        message(FATAL_ERROR "AudioToolbox is required for this platform")
    endif (NOT AUDIOTOOLBOX_LIBRARY)
    if (NOT AUDIOUNIT_LIBRARY)
        message(FATAL_ERROR "AudioUnit is required for this platform")
    endif (NOT AUDIOUNIT_LIBRARY)
    if (NOT CARBON_LIBRARY)
        message(FATAL_ERROR "Carbon is required for this platform")
    endif (NOT CARBON_LIBRARY)
    if (NOT COCOA_LIBRARY)
        message(FATAL_ERROR "Cocoa is required for this platform")
    endif (NOT COCOA_LIBRARY)
endif (APPLE)

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
add_subdirectory(${CMAKE_SOURCE_DIR}/src/table)
add_subdirectory(${CMAKE_SOURCE_DIR}/src/strgen)
add_subdirectory(${CMAKE_SOURCE_DIR}/src/lang)

include(SourceList REQUIRED)
source_list()

include(SourceList2 REQUIRED)
add_subdirectory(${CMAKE_SOURCE_DIR}/src)
CheckSourceEquality()

list(APPEND FIND_VERSION_FILES "${CMAKE_BINARY_DIR}/generated/rev.cpp")
if (WIN32)
    list(APPEND FIND_VERSION_FILES "${CMAKE_BINARY_DIR}/generated/ottdres.rc")
endif (WIN32)

# Generate a target to determine version, which is execute every 'make' run
add_custom_target(find_version
        ${CMAKE_COMMAND} -DFIND_VERSION_BINARY_DIR=${CMAKE_BINARY_DIR}/generated -P "${CMAKE_SOURCE_DIR}/cmake/scripts/FindVersion.cmake"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        BYPRODUCTS ${FIND_VERSION_FILES}
)
list(APPEND SOURCE_FILES "${FIND_VERSION_FILES}")

include(Endian REQUIRED)
add_endian_definition()

if (MSVC)
    # Switch to MT (static) instead of MD (dynamic) binary

    # For MSVC two generators are available
    # - a command line generator (Ninja) using CMAKE_BUILD_TYPE to specify the
    #   configuration of the build tree
    # - an IDE generator (Visual Studio) using CMAKE_CONFIGURATION_TYPES to
    #   specify all configurations that will be available in the generated solution
    list(APPEND MSVC_CONFIGS "${CMAKE_BUILD_TYPE}" "${CMAKE_CONFIGURATION_TYPES}")

    # Set usage of static runtime for all configurations
    foreach(MSVC_CONFIG ${MSVC_CONFIGS})
        string(TOUPPER "CMAKE_CXX_FLAGS_${MSVC_CONFIG}" MSVC_FLAGS)
        string(REPLACE "/MD" "/MT" ${MSVC_FLAGS} "${${MSVC_FLAGS}}")
    endforeach()

    # Add DPI manifest to project; other WIN32 targets get this via ottdres.rc
    list(APPEND SOURCE_FILES "${CMAKE_SOURCE_DIR}/os/windows/openttd.manifest")
endif (MSVC)

if (NOT MSVC)
    add_compile_options(-Wno-multichar)
endif (NOT MSVC)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src/3rdparty/squirrel/include)

add_executable(openttd
        WIN32
        ${SOURCE_FILES})

add_dependencies(openttd
        find_version)

target_link_libraries(openttd
    openttd::languages
    openttd::settings
    Threads::Threads
)

set_target_properties(openttd PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

if (APPLE OR UNIX)
    add_definitions(-DUNIX)
endif (APPLE OR UNIX)

link_library(PNG ENCOURAGED)
link_library(ZLIB ENCOURAGED)
link_library(LIBLZMA ENCOURAGED)
link_library(LZO ENCOURAGED)
link_library(XDG_BASEDIR)

if (NOT OPTION_DEDICATED)
    link_library(FLUIDSYNTH)
    link_library(SDL)
    link_library(ALLEGRO)
    link_library(FREETYPE)
    link_library(FONTCONFIG)
    link_library(ICU_lx)
    link_library(ICU_i18n)
endif (NOT OPTION_DEDICATED)

if (APPLE)
    link_library(Iconv)

    target_link_libraries(openttd ${AUDIOTOOLBOX_LIBRARY})
    target_link_libraries(openttd ${AUDIOUNIT_LIBRARY})
    target_link_libraries(openttd ${CARBON_LIBRARY})
    target_link_libraries(openttd ${COCOA_LIBRARY})

    add_definitions(-DWITH_COCOA)
    add_definitions(-DENABLE_COCOA_QUARTZ)
    add_definitions(-DNO_QUICKTIME)
endif (APPLE)

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

    if (NOT MSVC)
        # Add the correct (S)SSE flag for (S)SSE files
        file(GLOB SSE2_FILES ${CMAKE_SOURCE_DIR}/src/*_sse2.cpp ${CMAKE_SOURCE_DIR}/src/*/*_sse2.cpp)
        file(GLOB SSSE3_FILES ${CMAKE_SOURCE_DIR}/src/*_ssse3.cpp ${CMAKE_SOURCE_DIR}/src/*/*_ssse3.cpp)
        file(GLOB SSE4_FILES ${CMAKE_SOURCE_DIR}/src/*_sse4.cpp ${CMAKE_SOURCE_DIR}/src/*/*_sse4.cpp)
        set_source_files_properties(${SSE2_FILES} PROPERTIES COMPILE_FLAGS -msse2)
        set_source_files_properties(${SSSE3_FILES} PROPERTIES COMPILE_FLAGS -mssse3)
        set_source_files_properties(${SSE4_FILES} PROPERTIES COMPILE_FLAGS -msse4.1)
    endif (NOT MSVC)
endif (SSE_FOUND)

add_definitions_based_on_options()

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

add_custom_target(regression
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/scripts/Regression.cmake"
        DEPENDS openttd)

# Also make sure that 'make test' runs the regression
enable_testing()
add_test(NAME regression
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/scripts/Regression.cmake"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
