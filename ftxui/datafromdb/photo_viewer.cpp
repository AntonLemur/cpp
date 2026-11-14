#include "photo_viewer.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

void view_employee_photo_fullscreen(const Employee& emp) {
    if (!emp.hasPhoto || emp.photo.empty()) {
        return;
    }

#ifdef _WIN32
    // --- ВЕРСИЯ ДЛЯ WINDOWS ---
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    std::filesystem::path temp_file = temp_dir / ("emp_photo_" + std::to_string(emp.id) + ".jpg");
    std::string file_path = temp_file.string();

    std::ofstream out_file(file_path, std::ios::out | std::ios::binary);
    if (!out_file.is_open()) return;
    out_file.write(reinterpret_cast<const char*>(emp.photo.data()), emp.photo.size());
    out_file.close();

    // Открываем стандартным просмотрщиком Windows и ждём его закрытия (/wait)
    std::string cmd = "start \"\" /wait \"" + file_path + "\"";
    std::system(cmd.c_str());

    std::remove(file_path.c_str());

#else
    // --- ВЕРСИЯ ДЛЯ LINUX (ваш проверенный вариант с /dev/shm и fim) ---
    std::string ram_path = "/dev/shm/emp_photo_" + std::to_string(emp.id) + ".jpg";

    std::ofstream out_file(ram_path, std::ios::out | std::ios::binary);
    if (!out_file.is_open()) return;
    out_file.write(reinterpret_cast<const char*>(emp.photo.data()), emp.photo.size());
    out_file.close();

    bool is_graphical_env = (std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr);
    std::string fim_command;

    if (is_graphical_env) {
        fim_command = "fim -o sdl=W -c 'autoscale \"w\"' " + ram_path;
    } else {
        fim_command = "fim -o fb -a " + ram_path;
    }

    std::system(fim_command.c_str());
    std::remove(ram_path.c_str());
#endif
}
