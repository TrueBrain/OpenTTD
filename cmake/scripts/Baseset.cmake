cmake_minimum_required(VERSION 3.5)

# Retrieve the arguments (0 to 2 are for "cmake -P Baseset.cmake")
set(INPUTFILE ${CMAKE_ARGV3})
set(OUTPUTFILE ${CMAKE_ARGV4})
set(LANGFILES ${CMAKE_ARGV5})
set(ORIG_EXTRA_GRF ${CMAKE_ARGV6})

# Convert the command line compatible string into a real list
string(REPLACE "\\ " ";" LANGFILES ${LANGFILES})

# Place holder format is @<ini_key>_<str_id>@
file(STRINGS ${INPUTFILE} PLACE_HOLDER REGEX "^@")
string(REGEX REPLACE "@([^_]+).*@" "\\1" INI_KEY "${PLACE_HOLDER}")
string(REGEX REPLACE "@[^_]+_(.*)@" "\\1" STR_ID "${PLACE_HOLDER}")
string(REGEX REPLACE "@(.*)@" "\\1" PLACE_HOLDER "${PLACE_HOLDER}")

# Get the translations
foreach(LANGFILE IN LISTS LANGFILES)
    file(STRINGS ${LANGFILE} LANGLINES REGEX "^(##isocode|${STR_ID})" ENCODING UTF-8)
    string(FIND "${LANGLINES}" "${STR_ID}" HAS_STR_ID)
    if (HAS_STR_ID LESS 0)
        continue()
    endif (HAS_STR_ID LESS 0)
    string(REGEX REPLACE "##isocode ([^;]+).*" "\\1" ISOCODE "${LANGLINES}")
    if ("${ISOCODE}" STREQUAL "en_GB")
        string(REGEX REPLACE "[^:]*:(.*)" "${INI_KEY}       = \\1" LANGLINES "${LANGLINES}")
    else()
        string(REGEX REPLACE "[^:]*:(.*)" "${INI_KEY}.${ISOCODE} = \\1" LANGLINES "${LANGLINES}")
    endif()
    list(APPEND ${PLACE_HOLDER} ${LANGLINES})
endforeach(LANGFILE)
list(SORT ${PLACE_HOLDER})
list(JOIN ${PLACE_HOLDER} "\n" ${PLACE_HOLDER})

# Get the grf md5
file(MD5 ${ORIG_EXTRA_GRF} ORIG_EXTRA_GRF_MD5)

configure_file(${INPUTFILE} ${OUTPUTFILE})
