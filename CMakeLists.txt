cmake_minimum_required(VERSION 3.24)
project(DenshaDeKoi VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_DESKTOP_APP "Build the Raylib game and atlas inspector" ON)
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
    src/TableGame.cpp
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

    add_executable(denshadekoi src/TableMain.cpp src/TableScene.cpp src/TableView.cpp src/CardPresentation.cpp)
    configure_project_target(denshadekoi)
    target_link_libraries(denshadekoi PRIVATE raylib hanafuda_core)

    # Keep the earlier inspector available as a separate developer tool.
    add_executable(denshadekoi_inspector src/main.cpp src/AtlasManager.cpp)
    configure_project_target(denshadekoi_inspector)
    target_link_libraries(denshadekoi_inspector PRIVATE raylib hanafuda_core)
    # Both normally share an output directory; serialize asset writes there.
    add_dependencies(denshadekoi_inspector denshadekoi)

    # Target-specific builds must also receive their assets on the first build.
    add_custom_command(TARGET denshadekoi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png" "$<TARGET_FILE_DIR:denshadekoi>/cards_atlas.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/the-city.jpg" "$<TARGET_FILE_DIR:denshadekoi>/the-city.jpg"
        VERBATIM
    )
    add_custom_command(TARGET denshadekoi_inspector POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png" "$<TARGET_FILE_DIR:denshadekoi_inspector>/cards_atlas.png"
        VERBATIM
    )

    # Copy both assets beside the executables, including multi-config builds.
    # The custom target refreshes assets even when the executables do not relink.
    add_custom_target(update_game_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png"
            "$<TARGET_FILE_DIR:denshadekoi>/cards_atlas.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/the-city.jpg"
            "$<TARGET_FILE_DIR:denshadekoi>/the-city.jpg"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png"
            "$<TARGET_FILE_DIR:denshadekoi_inspector>/cards_atlas.png"
        DEPENDS denshadekoi denshadekoi_inspector
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    # Exercise the actual table rendering commands with a Raylib test double.
    add_executable(test_table tests/test_table.cpp src/TableScene.cpp src/TableView.cpp src/CardPresentation.cpp)
    configure_project_target(test_table)
    target_include_directories(test_table PRIVATE tests/fake_raylib src)
    target_link_libraries(test_table PRIVATE hanafuda_core)
    add_test(NAME table COMMAND test_table)

    add_executable(test_card_presentation tests/test_card_presentation.cpp src/CardPresentation.cpp src/TableView.cpp)
    configure_project_target(test_card_presentation)
    target_include_directories(test_card_presentation PRIVATE tests/fake_raylib src)
    target_link_libraries(test_card_presentation PRIVATE hanafuda_core)
    add_test(NAME card_presentation COMMAND test_card_presentation)

    # Run the real main loop against scripted, windowless Raylib input.
    add_library(table_main_test_entry OBJECT src/TableMain.cpp)
    configure_project_target(table_main_test_entry)
    target_compile_definitions(table_main_test_entry PRIVATE main=TableMainForTest DENSHA_TEST_SEED=5)
    target_include_directories(table_main_test_entry PRIVATE tests/fake_raylib src)
    add_executable(test_table_main tests/test_table_main.cpp src/TableScene.cpp src/TableView.cpp src/CardPresentation.cpp
        $<TARGET_OBJECTS:table_main_test_entry>)
    configure_project_target(test_table_main)
    target_include_directories(test_table_main PRIVATE tests/fake_raylib src)
    target_link_libraries(test_table_main PRIVATE hanafuda_core)
    add_test(NAME table_main COMMAND test_table_main)

    add_executable(test_table_game tests/test_table_game.cpp)
    configure_project_target(test_table_game)
    target_link_libraries(test_table_game PRIVATE hanafuda_core)
    add_test(NAME table_game COMMAND test_table_game)

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
