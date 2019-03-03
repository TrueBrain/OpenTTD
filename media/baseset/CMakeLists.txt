cmake_minimum_required(VERSION 3.5)

set(BASESET_BINARY_DIR ${CMAKE_BINARY_DIR}/baseset)
set(BASESET_SOURCE_DIR ${CMAKE_SOURCE_DIR}/media/baseset)
set(BASESET_LANG_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/lang)
set(BASESET_EXTRAGRF_FILE ${CMAKE_SOURCE_DIR}/bin/baseset/orig_extra.grf)

# Get all the (finished) language files
file(GLOB BASESET_LANG_SOURCE_FILES ${BASESET_LANG_SOURCE_DIR}/*.txt)

# Walk over all the baseset files, and generate a command to configure them
file(GLOB BASESET_SOURCE_FILES ${BASESET_SOURCE_DIR}/*.ob[smg])
foreach(BASESET_SOURCE_FILE IN LISTS BASESET_SOURCE_FILES)
    get_filename_component(BASESET_SOURCE_FILE_NAME ${BASESET_SOURCE_FILE} NAME)

    set(BASESET_BINARY_FILE ${BASESET_BINARY_DIR}/${BASESET_SOURCE_FILE_NAME})

    add_custom_command(OUTPUT ${BASESET_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/scripts/Baseset.cmake" "${BASESET_SOURCE_FILE}" "${BASESET_BINARY_FILE}" "${BASESET_LANG_SOURCE_DIR}" "${BASESET_EXTRAGRF_FILE}"
            MAIN_DEPENDENCY ${BASESET_SOURCE_FILE}
            DEPENDS ${BASESET_LANG_SOURCE_FILES} ${BASESET_EXTRAGRF_FILE} "${CMAKE_SOURCE_DIR}/cmake/scripts/Baseset.cmake"
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating ${BASESET_SOURCE_FILE_NAME} baseset metadata file"
    )

    list(APPEND BASESET_BINARY_FILES ${BASESET_BINARY_FILE})
endforeach(BASESET_SOURCE_FILE)

# Create a new target which generates all baseset metadata files
add_custom_target(baseset_files
        DEPENDS
        ${BASESET_BINARY_FILES}
)

add_library(basesets
        INTERFACE
)
add_dependencies(basesets
        baseset_files
)
add_library(openttd::basesets ALIAS basesets)
