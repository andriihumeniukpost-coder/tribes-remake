cmake_minimum_required(VERSION 3.10)
project(tribes_remake CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(tribes src/main.cpp src/gfx.cpp src/data.cpp src/game.cpp)

if(EMSCRIPTEN)
  # Сборка для браузера (iPad, Android-браузер, ПК): SDL2 встроен через порт Emscripten
  target_compile_options(tribes PRIVATE -sUSE_SDL=2)
  target_link_options(tribes PRIVATE
    -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB
    --preload-file ${CMAKE_SOURCE_DIR}/data@/data
    --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets)
  set_target_properties(tribes PROPERTIES SUFFIX ".html")
else()
  find_package(SDL2 REQUIRED CONFIG)
  target_link_libraries(tribes PRIVATE SDL2::SDL2)
endif()
