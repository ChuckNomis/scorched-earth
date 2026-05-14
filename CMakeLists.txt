cmake_minimum_required(VERSION 3.31)
project(bagel26)

set(CMAKE_CXX_STANDARD 20)

# bagel26: header-only ECS engine
add_library(bagel26 INTERFACE)
target_include_directories(bagel26 INTERFACE ${CMAKE_SOURCE_DIR})

set(SDL_STATIC ON)
set(SDL_SHARED OFF)
add_subdirectory(lib/SDL)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(lib/SDL_image)

# se: Scorched Earth game
add_executable(se
        main.cpp
        SE/ScorchedEarth.h
        SE/ScorchedEarth.cpp)

target_link_libraries(se PRIVATE bagel26 SDL3-static SDL3_image-static)

add_custom_command(TARGET se POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${PROJECT_SOURCE_DIR}/res"
        "$<TARGET_FILE_DIR:se>/res")
