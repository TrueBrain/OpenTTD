cmake_minimum_required(VERSION 3.5)

project(OpenTTD)

# Ensure we are at least C++11 compatible
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

add_subdirectory(${CMAKE_SOURCE_DIR}/src/settingsgen)
add_subdirectory(${CMAKE_SOURCE_DIR}/src/strgen)

find_package(ZLIB)
find_package(PNG)

file(READ ${CMAKE_SOURCE_DIR}/source.list SOURCE_LIST)
string(REGEX REPLACE "\n" ";" SOURCE_LIST "${SOURCE_LIST}")
string(REGEX REPLACE ";\t*" ";" SOURCE_LIST "${SOURCE_LIST}")
string(REGEX REPLACE "[;]+;" ";" SOURCE_LIST "${SOURCE_LIST}")
string(REGEX REPLACE ";$" "" SOURCE_LIST "${SOURCE_LIST}")

set(OPTION_WIN32 YES)
set(OPTION_OSX NO)
set(OPTION_SDL NO)
set(OPTION_WINCE NO)
set(OPTION_OS2 NO)
set(OPTION_LIBTIMIDITY NO)
set(OPTION_DIRECTMUSIC NO)
set(OPTION_PSP NO)
set(OPTION_SSE YES)
set(OPTION_ALLEGRO NO)
set(OPTION_DEDICATED NO)
set(OPTION_DOS NO)
set(OPTION_MORPHOS NO)
set(OPTION_BEOS NO)
set(OPTION_COCOA NO)
set(OPTION_HAVE_THREAD YES)

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

include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if (IS_BIG_ENDIAN)
    add_compile_definitions(TTD_ENDIAN=TTD_BIG_ENDIAN)
else (IS_BIG_ENDIAN)
    add_compile_definitions(TTD_ENDIAN=TTD_LITTLE_ENDIAN)
endif (IS_BIG_ENDIAN)
#list(APPEND SOURCE_FILES ${CMAKE_BINARY_DIR}/generated/rev.cpp)
#include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(openttd WIN32 ${SOURCE_FILES})
include_directories(${CMAKE_SOURCE_DIR}/src/3rdparty/squirrel/include)

if (NOT PNG_FOUND)
    message("libpng not found; functionality will be limited")
else(NOT PNG_FOUND)
    add_definitions(-DWITH_PNG)
    include_directories(${PNG_INCLUDE_DIR})
    target_link_libraries(openttd ${PNG_LIBRARIES})
endif (NOT PNG_FOUND)

if (NOT ZLIB_FOUND)
    message("zlib not found; functionality will be limited")
else(NOT ZLIB_FOUND)
    add_definitions(-DWITH_ZLIB)
    include_directories(${ZLIB_INCLUDE_DIR})
    target_link_libraries(openttd ${ZLIB_LIBRARIES})
endif (NOT ZLIB_FOUND)

target_link_libraries(openttd
        openttd::languages
        openttd::settings
        ws2_32 winmm gdi32 dxguid ole32 imm32
)
