cmake_minimum_required(VERSION 3.16)

project(graph VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Подключаем модуль для автоматической загрузки библиотеки
include(FetchContent)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0 # Используем стабильный релиз
)

FetchContent_MakeAvailable(ftxui)

add_executable(graph main.cpp
  README.md)

# Подключаем компоненты FTXUI (screen, dom, component)
target_link_libraries(graph
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
)

include(GNUInstallDirs)
install(TARGETS graph
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Включаем ThreadSanitizer (только для GCC/Clang на Linux и macOS)
option(ENABLE_TSAN "Включить ThreadSanitizer для поиска гонок данных" OFF)

if(ENABLE_TSAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(graph PRIVATE -fsanitize=thread -g -O1)
    target_link_options(graph PRIVATE -fsanitize=thread)
  else()
    message(WARNING "ThreadSanitizer поддерживается только в GCC и Clang")
  endif()
endif()
