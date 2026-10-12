cmake_minimum_required(VERSION 3.10)
project(RPG_Project LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Game rules and data, shared by the terminal game, tests and simulations.
add_library(rpg_core
    src/core/random.cpp
    src/character/character.cpp
    src/player/player.cpp src/player/jobs.cpp src/player/skills.cpp
    src/enemy/enemy.cpp
    src/item/item.cpp
    src/game/campaign.cpp src/game/combat.cpp src/game/world.cpp
    src/game/exploration.cpp src/game/exploration_text.cpp
    src/storage/save.cpp src/storage/profile.cpp
    src/ui/help.cpp)
target_include_directories(rpg_core PUBLIC src)
if(MSVC)
    target_compile_options(rpg_core PUBLIC /utf-8)
endif()
add_executable(RPG_Game src/main.cpp)
target_link_libraries(RPG_Game PRIVATE rpg_core)

include(CTest)
if(BUILD_TESTING)
    foreach(test_name random_tests enemy_tests skill_progression_tests save_tests combat_tests build_tests loot_tests campaign_tests profile_tests exploration_tests)
        add_executable(${test_name} tests/unit/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE rpg_core)
        if(MSVC)
            target_compile_options(${test_name} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${test_name} PRIVATE -UNDEBUG)
        endif()
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
    foreach(sim_name balance_sim campaign_sim)
        add_executable(${sim_name} tests/sim/${sim_name}.cpp)
        target_link_libraries(${sim_name} PRIVATE rpg_core)
    endforeach()
    add_test(NAME campaign_sim COMMAND campaign_sim --check)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME cli_smoke COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_smoke.py $<TARGET_FILE:RPG_Game>)
    endif()
endif()
