cmake_minimum_required(VERSION 3.24)
project(DenshaDeKoi VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_DESKTOP_APP "Build the Raylib desktop atlas inspector" ON)
include(CTest)

function(configure_project_target target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

if(BUILD_DESKTOP_APP)
    include(FetchContent)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)

    add_executable(denshadekoi
        src/main.cpp
        src/AtlasManager.cpp
    )
    configure_project_target(denshadekoi)
    target_link_libraries(denshadekoi PRIVATE raylib)

    # Keep the runtime asset alongside the executable, including multi-config builds.
    add_custom_command(TARGET denshadekoi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png"
            "$<TARGET_FILE_DIR:denshadekoi>/cards_atlas.png"
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    add_executable(test_atlas_math tests/test_atlas_math.cpp)
    configure_project_target(test_atlas_math)
    target_include_directories(test_atlas_math PRIVATE src)
    add_test(NAME atlas_math
        COMMAND test_atlas_math "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png")
endif()
