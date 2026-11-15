cmake_minimum_required(VERSION 3.16)

project(datafromdb VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Подключаем модуль для автоматической загрузки библиотеки
include(FetchContent)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0 # Используем стабильный релиз
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(ftxui)

# Подключаем SDL2
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.30.8
  GIT_SHALLOW TRUE
)

# Подключаем SDL2_image (отключаем лишние внешние зависимости, оставляем встроенные декодеры)
set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2_image
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
  GIT_TAG release-2.8.2
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(ftxui SDL2 SDL2_image)

add_executable(datafromdb main.cpp db_manager.h db_manager.cpp
  employee_service.h employee_service.cpp
  photo_renderer.h
  photo_renderer.cpp
  stb_image.h
  photo_viewer.h
  photo_viewer.cpp
  README.md
)

target_include_directories(datafromdb PRIVATE ${stb_SOURCE_DIR})

# Подключаем компоненты FTXUI (screen, dom, component)
target_link_libraries(datafromdb
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
  PRIVATE SDL2::SDL2
  PRIVATE SDL2_image::SDL2_image
  fbclient
)

# Для Windows: копируем собранные SDL2.dll и SDL2_image.dll рядом с .exe после сборки
if(WIN32)
  add_custom_command(TARGET datafromdb POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:SDL2::SDL2>
      $<TARGET_FILE:SDL2_image::SDL2_image>
      $<TARGET_FILE_DIR:datafromdb>
  )
endif()

include(GNUInstallDirs)
install(TARGETS datafromdb
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Включаем ThreadSanitizer (только для GCC/Clang на Linux и macOS)
option(ENABLE_TSAN "Включить ThreadSanitizer для поиска гонок данных" OFF)

if(ENABLE_TSAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(datafromdb PRIVATE -fsanitize=thread -g -O1)
    target_link_options(datafromdb PRIVATE -fsanitize=thread)
  else()
    message(WARNING "ThreadSanitizer поддерживается только в GCC и Clang")
  endif()
endif()
