cmake_minimum_required(VERSION 3.31)
project(bagel26)

set(CMAKE_CXX_STANDARD 20)

add_executable(bagel26 main.cpp
        bagel.h
        tests.cpp
        pong.h
        PongMockup.h
        Pong.cpp)

set(SDL_STATIC ON)
set(SDL_SHARED OFF)
add_subdirectory(lib/SDL)
target_link_libraries(bagel26 PUBLIC SDL3-static)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(lib/SDL_image)
target_link_libraries(bagel26 PUBLIC SDL3_image-static)

set(BOX2D_SAMPLES OFF)
set(BOX2D_BENCHMARKS OFF)
set(BOX2D_DOCS OFF)
set(BOX2D_PROFILE OFF)
set(BOX2D_VALIDATE OFF)
set(BOX2D_UNIT_TESTS OFF)
add_subdirectory(lib/box2d)
target_link_libraries(${PROJECT_NAME} PUBLIC box2d)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${PROJECT_SOURCE_DIR}/res"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res")
