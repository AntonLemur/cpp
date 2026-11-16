#ifndef PHOTO_VIEWER_H
#define PHOTO_VIEWER_H

#include "employee_service.h" // Заголовок, где объявлена структура Employee

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>

struct RawTerminalGuard {
    termios orig_termios{};
    bool active = false;

    RawTerminalGuard() {
        if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
            termios raw = orig_termios;
            raw.c_lflag &= ~(ECHO | ICANON); // Отключаем эхо и буферизацию строк
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
                active = true;
            }
        }
    }

    ~RawTerminalGuard() {
        if (active) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        }
    }
};
#endif // !_WIN32

// Открывает фотографию сотрудника во внешнем просмотрщике в полном разрешении.
// Перед вызовом из FTXUI необходимо обернуть в screen.WithRestoredIO([&] { ... }).
void view_employee_photo_fullscreen(const Employee& emp);
void view_employee_photo_from_memory(const Employee& emp);
#endif // PHOTO_VIEWER_H
