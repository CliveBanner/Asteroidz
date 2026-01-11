cmake_minimum_required(VERSION 3.16)
project(asteriodz)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find sources
file(GLOB SOURCES "src/*.c")

# This assumes SDL is vendored or available system-wide.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendored/SDL")
    add_subdirectory(vendored/SDL SDL_LIB EXCLUDE_FROM_ALL)
    set(SDL_TARGET SDL3::SDL3)
else()
    find_package(SDL3 REQUIRED)
    set(SDL_TARGET SDL3::SDL3)
endif()

# Define executable
add_executable(asteriodz ${SOURCES})

# Target properties
target_include_directories(asteriodz PRIVATE include)
target_link_libraries(asteriodz PRIVATE ${SDL_TARGET} m)