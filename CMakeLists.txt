cmake_minimum_required(VERSION 3.24)
project(DenshaDeKoi VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_DESKTOP_APP "Build the Raylib game and atlas inspector" ON)
include(CTest)

# Verify the exact owner-approved rules, table/card arrangement, and menu presentation on every build.
# This target is intentionally independent of Python and works with all supported
# CMake generators, including Visual Studio and MinGW.
add_custom_target(verify_protected_contract ALL
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/.github/protected-contract.sha256
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyProtectedContract.cmake
    COMMENT "Checking protected rules, card arrangement, and menu presentation"
    VERBATIM
)

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
    src/RadioPlaylist.cpp
    src/TableGame.cpp
)
configure_project_target(hanafuda_core)
add_dependencies(hanafuda_core verify_protected_contract)
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
        src/TableMain.cpp
        src/GameOptions.cpp
        src/TableScene.cpp
        src/ui/PauseRadioMenu.cpp
        src/ui/PauseMenu.cpp
        src/ui/TableHudRenderer.cpp
        src/RadioEngine.cpp
        src/SimpleRadio.cpp
        src/SfxPlayer.cpp
    )
    configure_project_target(denshadekoi)
    target_include_directories(denshadekoi PRIVATE "${raylib_SOURCE_DIR}/src")
    target_link_libraries(denshadekoi PRIVATE raylib hanafuda_core)
    if(WIN32)
        target_link_libraries(denshadekoi PRIVATE winhttp ws2_32 winmm ole32)
    else()
        find_package(Threads REQUIRED)
        target_link_libraries(denshadekoi PRIVATE Threads::Threads)
    endif()

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
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/koidenscha.png" "$<TARGET_FILE_DIR:denshadekoi>/koidenscha.png"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts" "$<TARGET_FILE_DIR:denshadekoi>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/options" "$<TARGET_FILE_DIR:denshadekoi>/options"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/sfx" "$<TARGET_FILE_DIR:denshadekoi>/sfx"
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
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/koidenscha.png"
            "$<TARGET_FILE_DIR:denshadekoi>/koidenscha.png"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
            "$<TARGET_FILE_DIR:denshadekoi>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/options" "$<TARGET_FILE_DIR:denshadekoi>/options"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/sfx"
            "$<TARGET_FILE_DIR:denshadekoi>/sfx"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/cards_atlas.png"
            "$<TARGET_FILE_DIR:denshadekoi_inspector>/cards_atlas.png"
        DEPENDS denshadekoi denshadekoi_inspector
        VERBATIM
    )
endif()

if(BUILD_TESTING)
    # Exercise the actual table rendering commands with a Raylib test double.
    add_executable(test_table tests/test_table.cpp tests/test_options.cpp src/GameOptions.cpp
        src/TableScene.cpp src/ui/PauseRadioMenu.cpp src/ui/PauseMenu.cpp src/ui/TableHudRenderer.cpp)
    configure_project_target(test_table)
    target_include_directories(test_table PRIVATE tests/fake_raylib src)
    target_link_libraries(test_table PRIVATE hanafuda_core)
    add_test(NAME table COMMAND test_table)
    add_test(NAME pause_options COMMAND test_table --options)

    add_executable(test_table_game tests/test_table_game.cpp)
    configure_project_target(test_table_game)
    target_link_libraries(test_table_game PRIVATE hanafuda_core)
    add_test(NAME table_game COMMAND test_table_game)

    add_executable(test_radio tests/test_radio.cpp src/RadioEngine.cpp)
    configure_project_target(test_radio)
    target_link_libraries(test_radio PRIVATE hanafuda_core)
    if(WIN32)
        target_link_libraries(test_radio PRIVATE winhttp ws2_32)
    else()
        find_package(Threads REQUIRED)
        target_link_libraries(test_radio PRIVATE Threads::Threads)
    endif()
    add_test(NAME radio COMMAND test_radio)

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
