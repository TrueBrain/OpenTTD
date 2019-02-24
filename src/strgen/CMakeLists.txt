cmake_minimum_required(VERSION 3.5)

project(strgen)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../../cmake")

set(sourcefiles
        strgen.cpp
        strgen_base.cpp
        ../core/alloc_func.cpp
        ../misc/getoptdata.cpp
        ../string.cpp
)
add_definitions(-DSTRGEN)
add_executable(strgen ${sourcefiles})

include(Endian REQUIRED)
add_endian_definition()

set(LANG_BINARY_DIR ${CMAKE_BINARY_DIR}/lang)
set(LANG_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/lang)

# Walk over all the (finished) language files, and generate a command to compile them
file(GLOB LANG_SOURCE_FILES ${LANG_SOURCE_DIR}/*.txt)
foreach(LANG_SOURCE_FILE IN LISTS LANG_SOURCE_FILES)
    get_filename_component(LANG_SOURCE_FILE_NAME_WE ${LANG_SOURCE_FILE} NAME_WE)

    set(LANG_BINARY_FILE ${LANG_BINARY_DIR}/${LANG_SOURCE_FILE_NAME_WE}.lng)

    add_custom_command(OUTPUT ${LANG_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${LANG_BINARY_DIR}
            COMMAND strgen
                    -s ${LANG_SOURCE_DIR}
                    -d ${LANG_BINARY_DIR}
                    ${LANG_SOURCE_FILE}
            DEPENDS strgen
            MAIN_DEPENDENCY ${LANG_SOURCE_FILE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Compiling language ${LANG_SOURCE_FILE_NAME_WE}"
    )

    list(APPEND LANG_BINARY_FILES ${LANG_BINARY_FILE})
endforeach(LANG_SOURCE_FILE)

# Create a new target which compiles all language files
add_custom_target(language_files
        DEPENDS
        ${LANG_BINARY_FILES}
)


set(GENERATED_BINARY_DIR ${CMAKE_BINARY_DIR}/generated)
set(TABLE_BINARY_DIR ${GENERATED_BINARY_DIR}/table)

# Generate a command and target to create the strings table
add_custom_command(OUTPUT ${TABLE_BINARY_DIR}/strings.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TABLE_BINARY_DIR}
        COMMAND strgen
                -s ${LANG_SOURCE_DIR}
                -d ${TABLE_BINARY_DIR}
        DEPENDS strgen ${LANG_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating table/strings.h"
)
add_custom_target(table_strings
        DEPENDS
        ${TABLE_BINARY_DIR}/strings.h
)

add_library(languages
        INTERFACE
)
target_include_directories(languages
        INTERFACE
        ${GENERATED_BINARY_DIR}
)
add_dependencies(languages
        language_files
        table_strings
)
add_library(openttd::languages ALIAS languages)
