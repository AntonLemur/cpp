#ifndef CURSOR_STREAMBUF_H
#define CURSOR_STREAMBUF_H

#ifdef _WIN32

#include <windows.h>
#include <iostream>
#include <streambuf>
#include <string>

// void hide_windows_console_cursor() {
//     HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
//     CONSOLE_CURSOR_INFO cursorInfo;
//     if (GetConsoleCursorInfo(hOut, &cursorInfo)) {
//         cursorInfo.bVisible = FALSE;
//         SetConsoleCursorInfo(hOut, &cursorInfo);
//     }
// }

// Фильтр, который вырезает команду показа системного курсора (\033[?25h) перед отправкой в cmd.exe
class FastWinConsoleBuf : public std::streambuf {
public:
    std::streambuf* orig_buf;
    HANDLE hOut;
    std::string frame_buffer;
    bool allow_cursor = false; // Управляется из вашего Renderer

    FastWinConsoleBuf(std::streambuf* orig) : orig_buf(orig) {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        // Включаем обработку ANSI-последовательностей в Windows 10
        DWORD mode = 0;
        GetConsoleMode(hOut, &mode);
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    ~FastWinConsoleBuf() {
        if (orig_buf && std::cout.rdbuf() == this) {
            std::cout.rdbuf(orig_buf);
        }
    }

protected:
    // Блочная запись без тормозов (вместо посимвольного overflow)
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        frame_buffer.append(s, n);
        return n;
    }

    int overflow(int c) override {
        if (c != EOF) frame_buffer.push_back(static_cast<char>(c));
        return c;
    }

    // Вызывается FTXUI ровно 1 раз в конце отрисовки всего кадра
    int sync() override {
        if (!frame_buffer.empty()) {
            // 1. Прячем курсор в самом начале пакета
            // 2. Выводим весь кадр разом
            // 3. Курсор появится только в самом конце (в активном Input), не прыгая по экрану
            std::string packet = "\033[?25l" + frame_buffer;

            // Если сейчас не должно быть активного курсора ни в одном Input —
            // вырезаем все команды включения курсора (\033[?25h) из пакета
            if (!allow_cursor) {
                size_t pos = 0;
                while ((pos = packet.find("\033[?25h", pos)) != std::string::npos) {
                    packet.erase(pos, 6);
                }
            }

            DWORD written = 0;
            WriteConsoleA(hOut, packet.data(), static_cast<DWORD>(packet.size()), &written, nullptr);
            frame_buffer.clear();

            // Страховка для пустого Input с placeholder в FTXUI 5.0.0:
            // если курсор улетел в последнюю строку экрана — прячем его
            if (allow_cursor) {
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
                    // В Fullscreen-буфере правый нижний угол имеет координаты (Right, Bottom) или (Right - 1, Bottom - 1)
                    bool in_bottom_corner = (csbi.dwCursorPosition.Y >= csbi.srWindow.Bottom - 1) &&
                                            (csbi.dwCursorPosition.X >= csbi.srWindow.Right - 1);
                    if (in_bottom_corner) {
                        DWORD w = 0;
                        WriteConsoleA(hOut, "\033[?25l", 6, &w, nullptr);
                    }
                }
            }
        }
        return 0;
    }
};
#endif //#ifdef _WIN32

#endif // CURSOR_STREAMBUF_H
