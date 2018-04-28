cmake_minimum_required(VERSION 3.5)

project(settingsgen)

set(sourcefiles
        settingsgen.cpp
        ../core/alloc_func.cpp
        ../misc/getoptdata.cpp
        ../ini_load.cpp
        ../string.cpp
)
add_definitions(-DSETTINGSGEN)
add_executable(settingsgen ${sourcefiles})


set(GENERATED_BINARY_DIR ${CMAKE_BINARY_DIR}/generated)
set(TABLE_BINARY_DIR ${GENERATED_BINARY_DIR}/table)

# Generate a command and target to create the settings table
file(GLOB TABLE_INI_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/table/*.ini)
add_custom_command(OUTPUT ${TABLE_BINARY_DIR}/settings.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TABLE_BINARY_DIR}
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/settingsgen
                -o ${TABLE_BINARY_DIR}/settings.h
                -b ${CMAKE_SOURCE_DIR}/src/table/settings.h.preamble
                -a ${CMAKE_SOURCE_DIR}/src/table/settings.h.postamble
                ${TABLE_INI_SOURCE_FILES}
        DEPENDS settingsgen ${TABLE_INI_SOURCE_FILES}
        COMMENT "Generating table/settings.h"
)
add_custom_target(table_settings
        DEPENDS
        ${TABLE_BINARY_DIR}/settings.h
)

add_library(settings
        INTERFACE
)
target_include_directories(settings
        INTERFACE
        ${GENERATED_BINARY_DIR}
)
add_dependencies(settings
        table_settings
)
add_library(openttd::settings ALIAS settings)
