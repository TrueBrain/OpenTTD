cmake_minimum_required(VERSION 3.5)

set(BASESET_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_dos.obg
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_dos_de.obg
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_win.obg
        ${CMAKE_CURRENT_SOURCE_DIR}/no_music.obm
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_dos.obm
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_tto.obm
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_win.obm
        ${CMAKE_CURRENT_SOURCE_DIR}/no_sound.obs
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_dos.obs
        ${CMAKE_CURRENT_SOURCE_DIR}/orig_win.obs
)
set(BASESET_EXTRAGRF_FILE ${CMAKE_SOURCE_DIR}/bin/baseset/orig_extra.grf)

# Walk over all the baseset files, and generate a command to configure them
foreach(BASESET_SOURCE_FILE IN LISTS BASESET_SOURCE_FILES)
    get_filename_component(BASESET_SOURCE_FILE_NAME ${BASESET_SOURCE_FILE} NAME)

    set(BASESET_BINARY_FILE ${CMAKE_BINARY_DIR}/baseset/${BASESET_SOURCE_FILE_NAME})

    add_custom_command(OUTPUT ${BASESET_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/scripts/Baseset.cmake
                    ${BASESET_SOURCE_FILE}
                    ${BASESET_BINARY_FILE}
                    "$<JOIN:$<TARGET_PROPERTY:language_files,LANG_SOURCE_FILES>,\\ >" # Make the list command line compatible
                    ${BASESET_EXTRAGRF_FILE}
            MAIN_DEPENDENCY ${BASESET_SOURCE_FILE}
            DEPENDS $<TARGET_PROPERTY:language_files,LANG_SOURCE_FILES>
                    ${BASESET_EXTRAGRF_FILE}
                    ${CMAKE_SOURCE_DIR}/cmake/scripts/Baseset.cmake
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
