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
    get_target_property(target_type ${target} TYPE)
    if(MINGW AND target_type STREQUAL "EXECUTABLE")
        # Link the GCC runtime statically so the .exe also runs on PCs without MSYS2
        # (otherwise it needs libstdc++-6.dll, libgcc_s_seh-1.dll, libwinpthread-1.dll).
        target_link_options(${target} PRIVATE -static)
    endif()
endfunction()

# The card database and deck have no Raylib or window-system dependency.
add_library(hanafuda_core STATIC
    src/Deck.cpp
    src/KoiKoiRules.cpp
    src/KoiKoiStateMachine.cpp
)
configure_project_target(hanafuda_core)
target_compile_features(hanafuda_core PUBLIC cxx_std_20)
target_include_directories(hanafuda_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

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
    target_link_libraries(denshadekoi PRIVATE raylib hanafuda_core)

    # Keep the runtime asset alongside the executable, including multi-config builds.
    add_custom_command(TARGET denshadekoi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png"
            "$<TARGET_FILE_DIR:denshadekoi>/cards_atlas.png"
        VERBATIM
    )
    # Also refresh the asset on ordinary builds when only the PNG has changed.
    add_custom_target(update_card_atlas ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png"
            "$<TARGET_FILE_DIR:denshadekoi>/cards_atlas.png"
        DEPENDS denshadekoi
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    add_executable(test_rules tests/test_rules.cpp)
    configure_project_target(test_rules)
    target_link_libraries(test_rules PRIVATE hanafuda_core)
    add_test(NAME rules COMMAND test_rules)

    add_executable(test_deck tests/test_deck.cpp)
    configure_project_target(test_deck)
    target_link_libraries(test_deck PRIVATE hanafuda_core)
    add_test(NAME deck COMMAND test_deck)

    add_executable(test_atlas_math tests/test_atlas_math.cpp)
    configure_project_target(test_atlas_math)
    target_include_directories(test_atlas_math PRIVATE src)
    add_test(NAME atlas_math
        COMMAND test_atlas_math "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png")

    # If Python is available, also inspect the actual PNG pixels in every frame.
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME atlas_frames COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_atlas_frames.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png")
    endif()

    # Exercise the real inspector and AtlasManager with a lightweight Raylib test double.
    add_library(inspector_test_main OBJECT src/main.cpp)
    configure_project_target(inspector_test_main)
    target_compile_definitions(inspector_test_main PRIVATE main=InspectorMain)
    target_include_directories(inspector_test_main PRIVATE tests/fake_raylib src)

    add_executable(test_inspector tests/test_inspector.cpp src/AtlasManager.cpp
        $<TARGET_OBJECTS:inspector_test_main>)
    configure_project_target(test_inspector)
    target_include_directories(test_inspector PRIVATE tests/fake_raylib src)
    add_test(NAME inspector COMMAND test_inspector)
endif()
