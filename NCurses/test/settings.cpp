#define _XOPEN_SOURCE_EXTENDED 1
#define NCURSES_WIDECHAR 1

#include "settings.hpp"
#include "popup.hpp"
#include <ncursesw/ncurses.h>
#include <cwchar>
#include <string>
#include <vector>

// Структура для управления кастомным полем ввода
struct CustomField {
    std::string label;
    std::wstring value; // Храним текст в wide-строке для точного подсчета русских букв
    int y;              // Локальный Y внутри окна
    int x;              // Локальный X внутри окна
    int max_len;
};

void show_settings() {
    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    int width = 50;
    int height = 14; 
    int starty = (screen_rows - height) / 2;
    int startx = (screen_cols - width) / 2;

    PANEL *settings_panel = create_popup_window(height, width, starty, startx, "НАСТРОЙКИ");
    WINDOW *settings_win = panel_window(settings_panel);
    keypad(settings_win, TRUE);

    // Описываем наши поля
    std::vector<CustomField> fields = {
        {"Ваше имя:",  L"", 4, 4, 25},
        {"Ваш город:", L"", 8, 4, 25}
    };

    int active_field = 0; // Индекс текущего поля (0 - Имя, 1 - Город)
    bool editing = true;
    bool need_redraw = true; // 🌟 Флаг, управляющий отрисовкой

    wint_t ch;
    int key_type;

    while (editing) {
        // 🌟 ОТРИСОВКА СРАБОТАЕТ ТОЛЬКО ПРИ ИЗМЕНЕНИЯХ
        if (need_redraw) {
            // Рисуем статический текст
            mvwprintw(settings_win, 2, 4, "%s", fields[0].label.c_str());
            mvwprintw(settings_win, 6, 4, "%s", fields[1].label.c_str());

            // Отрисовываем поля
            for (size_t i = 0; i < fields.size(); i++) {
                if ((int)i == active_field) {
                    wattron(settings_win, COLOR_PAIR(4) | A_UNDERLINE | A_BOLD);
                } else {
                    wattron(settings_win, COLOR_PAIR(4) | A_UNDERLINE);
                }

                std::wstring display_str = fields[i].value;
                if (display_str.length() < (size_t)fields[i].max_len) {
                    display_str.append((size_t)fields[i].max_len - display_str.length(), L' ');
                }

                mvwaddnwstr(settings_win, fields[i].y, fields[i].x, display_str.c_str(), fields[i].max_len);
                wattroff(settings_win, COLOR_PAIR(4) | A_UNDERLINE);
                if ((int)i == active_field) wattroff(settings_win, A_BOLD);
            }

            // Сбрасываем флаг, так как экран теперь актуален
            need_redraw = false; 
        }

        // Перемещение курсора и апдейт панелей нужны на каждом шаге (так как курсор мигает)
        wmove(settings_win, fields[active_field].y, fields[active_field].x + fields[active_field].value.length());
        top_panel(settings_panel);
        update_panels();
        doupdate();

        // Блокирующий вызов — ждем нажатия клавиши
        key_type = wget_wch(settings_win, &ch);
        if (key_type == ERR) continue;

        if (key_type == OK) {
            if (ch == 10) { 
                editing = false;
            } else if (ch == 9) { // Tab
                active_field = (active_field + 1) % fields.size();
                need_redraw = true; // 🌟 Изменился фокус — перерисовываем
            } else if (ch == 127 || ch == 8) { // Backspace
                if (!fields[active_field].value.empty()) {
                    fields[active_field].value.pop_back();
                    need_redraw = true; // 🌟 Изменился текст — перерисовываем
                }
            } else if (ch >= 32) { // Символ
                if (fields[active_field].value.length() < (size_t)fields[active_field].max_len) {
                    fields[active_field].value.push_back(ch);
                    need_redraw = true; // 🌟 Добавился текст — перерисовываем
                }
            }
        } else if (key_type == KEY_CODE_YES) {
            // 🌟 ПЕРЕХВАТ КЛИКА МЫШИ В НАСТРОЙКАХ
            if (ch == KEY_MOUSE) {
                MEVENT mouse_event;
                if (getmouse(&mouse_event) == OK) {
                    // Проверяем нажатие левой кнопки
                    if (mouse_event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                        // Переводим глобальные координаты клика экрана в локальные координаты нашей панели
                        int local_y = mouse_event.y - starty;
                        int local_x = mouse_event.x - startx;

                        // Проверяем, попал ли клик по длине поля (от X=4 до X=29)
                        if (local_x >= 4 && local_x <= 29) {
                            // Если кликнули по строке 4 (поле Имя)
                            if (local_y == 4 && active_field != 0) {
                                active_field = 0;
                                need_redraw = true;
                            }
                            // Если кликнули по строке 8 (поле Город)
                            else if (local_y == 8 && active_field != 1) {
                                active_field = 1;
                                need_redraw = true;
                            }
                        }
                    }
                }
            }
            
            // Клавиатурная навигация (стрелочки)        
            switch (ch) {
                case KEY_BTAB: // Shift + Tab
                case KEY_UP:
                    active_field = (active_field - 1 + fields.size()) % fields.size();
                    need_redraw = true; // 🌟 Изменился фокус
                    break;
                case KEY_DOWN:
                    active_field = (active_field + 1) % fields.size();
                    need_redraw = true; // 🌟 Изменился фокус
                    break;
                case KEY_BACKSPACE:
                    if (!fields[active_field].value.empty()) {
                        fields[active_field].value.pop_back();
                        need_redraw = true;
                    }
                    break;
            }
        }
    }
    
    // Конвертируем финальные широкие строки обратно в UTF-8 std::string для вывода
    char username[100] = {0};
    char usercity[100] = {0};
    wcstombs(username, fields[0].value.c_str(), sizeof(username));
    wcstombs(usercity, fields[1].value.c_str(), sizeof(usercity));

    // Вывод результатов
    wclear(settings_win);
    wattron(settings_win, COLOR_PAIR(3) | A_BOLD); 
    box(settings_win, 0, 0);
    mvwprintw(settings_win, 0, (width - 22) / 2, " НАСТРОЙКИ СОХРАНЕНЫ ");
    wattroff(settings_win, COLOR_PAIR(3) | A_BOLD);

    mvwprintw(settings_win, 4, 4, "Привет, %s!", username);
    mvwprintw(settings_win, 6, 4, "Ваш город: %s", usercity);
    mvwprintw(settings_win, 9, 4, "Нажмите Enter для выхода...");
    
    update_panels();
    doupdate();
    
    while ((key_type = wget_wch(settings_win, &ch)) != ERR) {
        if (key_type == OK && ch == 10) break;
    }

    destroy_popup_window(settings_panel);
    update_panels();
    doupdate();
}

