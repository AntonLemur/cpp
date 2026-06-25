#include "photo_viewer.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <ncurses.h>

void view_employee_photo(const Employee& emp) {
    if (!emp.hasPhoto || emp.photo.empty()) {
        return; 
    }

    std::stringstream path_ss;
    path_ss << "/dev/shm/emp_photo_" << emp.id << ".jpg";
    std::string ram_path = path_ss.str();

    std::ofstream out_file(ram_path, std::ios::out | std::ios::binary);
    if (!out_file.is_open()) return;
    
    out_file.write(reinterpret_cast<const char*>(emp.photo.data()), emp.photo.size());
    out_file.close();

    // --- ВРЕМЕННО ОТКЛЮЧАЕМ NCURSES ---
    def_prog_mode(); 
    endwin();        

    bool is_graphical_env = (std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr);
    std::string fim_command;

    if (is_graphical_env) {
        // РЕШЕНИЕ ДЛЯ ЭМУЛЯТОРА ТЕРМИНАЛА (WAYLAND / MATE):
        // Формируем команду: запуск в SDL full-screen c авто-масштабированием
        fim_command = "fim -o sdl=W -c 'autoscale \"w\"' " + ram_path;
    } else {
        // РЕШЕНИЕ ДЛЯ ЧИСТОГО TTY (КОНСОЛИ):
        // Родной framebuffer, по умолчанию на весь экран
        fim_command = "fim -o fb -a " + ram_path;
    }

    // Выполняем выбранную команду
    // Поток C++ жестко заблокируется, пока вы не нажмете 'q' внутри полноэкранного фото
    std::system(fim_command.c_str());

    // --- ВОЗВРАЩАЕМ NCURSES НАЗАД ---
    reset_prog_mode(); 
    refresh();         

    std::remove(ram_path.c_str());
}

