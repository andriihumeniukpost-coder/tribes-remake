cmake_minimum_required(VERSION 3.22)
project(tribes_android CXX)
set(CMAKE_CXX_STANDARD 17)

# Корень проекта: android/app/src/main/cpp → ../../../../../
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

# SDL2 собирается из исходников (клон в cpp/SDL, делает CI)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL)

# Игра: та же логика, что и на ПК. Библиотека называется main, её загружает SDLActivity.
add_library(main SHARED
    ${ROOT}/src/main.cpp
    ${ROOT}/src/gfx.cpp
    ${ROOT}/src/data.cpp
    ${ROOT}/src/game.cpp)
target_include_directories(main PRIVATE ${ROOT}/src ${ROOT}/third_party)
target_link_libraries(main PRIVATE SDL2::SDL2 log)
