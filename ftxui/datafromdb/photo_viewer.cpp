#include "photo_viewer.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#define SDL_MAIN_HANDLED // Обязательно до подключения SDL.h для консольных программ в Windows
#include <SDL.h>
#include <SDL_image.h>

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

void view_employee_photo_from_memory(const Employee& emp) {
    if (!emp.hasPhoto || emp.photo.empty()) {
        return;
    }

    SDL_SetMainReady(); // Сообщаем SDL, что мы сами управляем функцией main()

    if (SDL_Init(SDL_INIT_VIDEO) == 0) {
        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

        SDL_Window* window = SDL_CreateWindow(
            "Просмотр фото (ESC или Q для выхода)",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            800, 600,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
            );

        if (window) {
            SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            SDL_RWops* rw = SDL_RWFromConstMem(emp.photo.data(), static_cast<int>(emp.photo.size()));
            SDL_Texture* texture = IMG_LoadTexture_RW(renderer, rw, 1);

            if (texture) {
                int img_w, img_h;
                SDL_QueryTexture(texture, nullptr, nullptr, &img_w, &img_h);

// В Linux TTY скрываем системный курсор мыши поверх картинки
#ifndef _WIN32
                SDL_ShowCursor(SDL_DISABLE);
#endif

                bool running = true;
                SDL_Event event;
                while (running) {
                    while (SDL_PollEvent(&event)) {
                        if (event.type == SDL_QUIT ||
                            (event.type == SDL_KEYDOWN &&
                             (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q))) {
                            running = false;
                        }
                    }

                    int win_w, win_h;
                    SDL_GetRendererOutputSize(renderer, &win_w, &win_h);
                    float scale = std::min((float)win_w / img_w, (float)win_h / img_h);
                    SDL_Rect dst = {
                        (int)((win_w - img_w * scale) / 2),
                        (int)((win_h - img_h * scale) / 2),
                        (int)(img_w * scale),
                        (int)(img_h * scale)
                    };

                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, nullptr, &dst);
                    SDL_RenderPresent(renderer);
                    SDL_Delay(16);
                }
                SDL_DestroyTexture(texture);
            }
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
        }
        IMG_Quit();
        SDL_Quit();
    }
}
