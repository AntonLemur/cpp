#include "photo_viewer.h"

#include <algorithm> // Нужно для std::min в Windows (MSVC)
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#ifndef _WIN32
#include <poll.h>
#endif

#define SDL_MAIN_HANDLED // Обязательно до подключения SDL.h для консольных программ в Windows
#include <SDL.h>
#include <SDL_image.h>

#ifndef _WIN32
#include <X11/Xlib.h>

struct TermBounds {
    int x = 100;
    int y = 100;
    int w = 800;
    int h = 600;
    bool found = false;
};

TermBounds get_terminal_window_bounds() {
    TermBounds res;
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return res;

    Window focus_win = None;
    int revert_to = 0;
    XGetInputFocus(dpy, &focus_win, &revert_to);

    if (focus_win != None && focus_win != PointerRoot) {
        Window root_win = DefaultRootWindow(dpy);
        Window parent_win = None;
        Window* children = nullptr;
        unsigned int nchildren = 0;

        // Поднимаемся к верхнеуровневому окну (рамке окна терминала)
        Window top_win = focus_win;
        while (true) {
            if (XQueryTree(dpy, top_win, &root_win, &parent_win, &children, &nchildren) == 0) break;
            if (children) XFree(children);
            if (parent_win == root_win || parent_win == None) break;
            top_win = parent_win;
        }

        int x_rel = 0, y_rel = 0;
        unsigned int w = 0, h = 0, border_w = 0, depth = 0;
        if (XGetGeometry(dpy, top_win, &root_win, &x_rel, &y_rel, &w, &h, &border_w, &depth)) {
            Window child_dummy = None;
            int abs_x = 0, abs_y = 0;
            // Переводим координаты в глобальные экранные
            if (XTranslateCoordinates(dpy, top_win, root_win, 0, 0, &abs_x, &abs_y, &child_dummy)) {
                res.x = abs_x;
                res.y = abs_y;
                res.w = static_cast<int>(w);
                res.h = static_cast<int>(h);
                res.found = true;
            }
        }
    }
    XCloseDisplay(dpy);
    return res;
}
#endif

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

    // Отключаем перехват клавиатуры через evdev, чтобы не требовалась группа input
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

    SDL_SetMainReady(); // Сообщаем SDL, что мы сами управляем функцией main()

    if (SDL_Init(SDL_INIT_VIDEO) == 0) {
        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

#ifdef _WIN32
        Uint32 window_flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALWAYS_ON_TOP;
        SDL_Window* window = SDL_CreateWindow(
            "Просмотр фото (ESC или Q для выхода)",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            0, 0, window_flags
            );
#else
        TermBounds bounds = get_terminal_window_bounds();
        // Если мы в графическом X11 и нашли окно терминала — перекрываем только его без рамок
        Uint32 window_flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS;
        SDL_Window* window = SDL_CreateWindow(
            "Просмотр фото (ESC или Q для выхода)",
            bounds.found ? bounds.x : SDL_WINDOWPOS_CENTERED,
            bounds.found ? bounds.y : SDL_WINDOWPOS_CENTERED,
            bounds.w, bounds.h,
            window_flags
            );
#endif

        if (window) {
            // 3. Показываем окно только после того, как оно уже настроено на полный экран
            SDL_ShowWindow(window);

#ifdef _WIN32
            // 1. Принудительно поднимаем окно поверх консоли и забираем фокус клавиатуры
            SDL_RaiseWindow(window);
            SDL_SetWindowInputFocus(window);

            // 2. Очищаем очередь событий (убираем хвост от нажатия клавиши 'f' и инициализации)
            SDL_PumpEvents();
            SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
#endif

            SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            SDL_RWops* rw = SDL_RWFromConstMem(emp.photo.data(), static_cast<int>(emp.photo.size()));
            SDL_Texture* texture = IMG_LoadTexture_RW(renderer, rw, 1);

            if (texture) {
                int img_w, img_h;
                SDL_QueryTexture(texture, nullptr, nullptr, &img_w, &img_h);

// В Linux TTY скрываем системный курсор мыши поверх картинки
#ifndef _WIN32
                SDL_ShowCursor(SDL_DISABLE);
                // Переводим stdin в неблокирующий raw-режим
                RawTerminalGuard term_guard;
                struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
#endif

                bool running = true;
                SDL_Event event;
                while (running) {
                    // Проверяем событие окна SDL (сработает в X11/Wayland / эмуляторе терминала)
                    while (SDL_PollEvent(&event)) {
                        if (event.type == SDL_QUIT ||
                            (event.type == SDL_KEYDOWN &&
                             (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q))) {
                            running = false;
                        }
                    }

#ifndef _WIN32
                    // Проверяем stdin терминала (сработает в голом TTY без группы input)
                    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
                        char ch = 0;
                        if (read(STDIN_FILENO, &ch, 1) > 0) {
                            // 27 — код клавиши Esc, 'q' / 'Q' — выход
                            if (ch == 27 || ch == 'q' || ch == 'Q') {
                                // Если это Esc, подождем 10 мс: вдруг это начало стрелки (\033[A)
                                if (ch == 27 && poll(&pfd, 1, 10) > 0) {
                                    char seq[8];
                                    read(STDIN_FILENO, seq, sizeof(seq)); // Сбрасываем хвост спецклавиши
                                } else {
                                    running = false;
                                }
                            }
                        }
                    }
#endif

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
