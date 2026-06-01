cmake_minimum_required(VERSION 3.16)
project(ProjectNovaCombat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Combat + monster logic is intentionally SDL-free so it can be unit-tested
# and reused. Headers are included relative to src/ (e.g. "combat/Hitbox.h").
add_library(nova_combat
    src/combat/CombatSystem.cpp
    src/monster/MonsterAttackController.cpp
    src/monster/Monster.cpp
    src/player/Player.cpp
    src/boss/BossPatternController.cpp
    src/boss/BossPatternSelector.cpp
)
target_include_directories(nova_combat PUBLIC src)

# Simple console demo of the melee attack flow.
add_executable(melee_demo examples/melee_demo.cpp)
target_link_libraries(melee_demo PRIVATE nova_combat)

# Simple console demo of the data-driven boss pattern loop.
add_executable(boss_demo examples/boss_demo.cpp)
target_link_libraries(boss_demo PRIVATE nova_combat)
