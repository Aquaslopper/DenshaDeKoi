if(NOT DEFINED SOURCE_DIR OR NOT DEFINED MANIFEST)
    message(FATAL_ERROR "SOURCE_DIR and MANIFEST are required")
endif()

if(NOT EXISTS "${MANIFEST}")
    message(FATAL_ERROR "Protected-contract manifest is missing: ${MANIFEST}")
endif()

file(STRINGS "${MANIFEST}" entries)
set(checked 0)
foreach(entry IN LISTS entries)
    if(entry STREQUAL "" OR entry MATCHES "^#")
        continue()
    endif()

    if(NOT entry MATCHES "^([0-9a-fA-F]+)  (.+)$")
        message(FATAL_ERROR "Invalid protected-contract entry: ${entry}")
    endif()

    set(expected "${CMAKE_MATCH_1}")
    string(LENGTH "${expected}" hash_length)
    if(NOT hash_length EQUAL 64)
        message(FATAL_ERROR "Protected-contract hash must contain 64 hexadecimal characters: ${entry}")
    endif()
    string(TOLOWER "${expected}" expected)
    set(relative_path "${CMAKE_MATCH_2}")
    set(full_path "${SOURCE_DIR}/${relative_path}")

    if(NOT EXISTS "${full_path}")
        message(FATAL_ERROR "Protected file is missing: ${relative_path}")
    endif()

    # Git checks text files out with CRLF on Windows runners. Hash normalized
    # content so the same approved source passes on Windows, Linux, and macOS.
    file(READ "${full_path}" content)
    string(REPLACE "\r\n" "\n" content "${content}")
    string(SHA256 actual "${content}")
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR
            "PROTECTED CONTRACT CHANGED: ${relative_path}\n"
            "Expected: ${expected}\n"
            "Actual:   ${actual}\n"
            "The executable was not built. Rules or card arrangement may only be changed "
            "with explicit owner approval and a deliberately updated baseline.")
    endif()
    math(EXPR checked "${checked} + 1")
endforeach()

if(checked EQUAL 0)
    message(FATAL_ERROR "Protected-contract manifest contains no files")
endif()

message(STATUS "Protected contract verified (${checked} files): rules, card arrangement, and menu presentation unchanged")
