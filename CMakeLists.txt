cmake_minimum_required(VERSION 3.16)
project(asteriodz)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This assumes SDL is vendored or available system-wide.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendored/SDL")
    add_subdirectory(vendored/SDL SDL_LIB EXCLUDE_FROM_ALL)
    set(SDL_TARGET SDL3::SDL3)
else()
    find_package(SDL3 REQUIRED)
    set(SDL_TARGET SDL3::SDL3)
endif()

# Define executable
add_executable(asteriodz
    src/main.c
    src/game.c
    src/input.c
    src/renderer.c
    src/utils.c
    src/physics.c
    src/particles.c
    src/ai.c
    src/persistence.c
    src/assets.c
    src/ui.c
    src/workers.c
    src/weapons.c
    src/abilities.c
)

# Target properties
target_include_directories(asteriodz PRIVATE include)
target_link_libraries(asteriodz PRIVATE ${SDL_TARGET} m)