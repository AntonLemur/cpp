cmake_minimum_required(VERSION 3.10)
project(MyTUIProject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

# Находим пакеты стандартным способом без создания внутренних таргетов
# Все нужные библиотеки и пути запишутся в переменную NCURSES_ALL
pkg_check_modules(NCURSES_ALL REQUIRED ncursesw panelw menuw formw)

# Перечисляем все файлы исходного кода через пробел
add_executable(test main.cpp popup.cpp settings.cpp dataview.cpp db_manager.cpp employee_service.cpp photo_viewer.cpp)

# Подключаем папки с заголовочными файлами (.h)
target_include_directories(test PRIVATE ${NCURSES_ALL_INCLUDE_DIRS})

# Линкуем библиотеки в правильном порядке.
# В переменной NCURSES_ALL_LIBRARIES библиотеки уже лежат в верном для Linux порядке,
# но для Debian 13 мы также принудительно подключаем их явные имена.
target_link_libraries(test PRIVATE 
    panelw
    menuw
    formw
    ncursesw
    fbclient 
    ncurses
)

