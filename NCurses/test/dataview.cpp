#define _XOPEN_SOURCE_EXTENDED 1
#define NCURSES_WIDECHAR 1

#include "dataview.hpp"
#include "popup.hpp"
#include <ncursesw/ncurses.h>
#include <ncursesw/panel.h>
#include <vector>
#include <string>
#include <cwchar>

struct DbRecord {
    std::wstring id;
    std::wstring name;
    std::wstring city;
    int age;
};

// Оптимизированная функция: рисует ТОЛЬКО ползунок, не трогая дорожку
void draw_scrollbar_thumb(WINDOW *win, int menu_height, int total_items, int current_index, int start_x) {
    if (total_items <= menu_height) return; 

    int track_y_start = 3; 
    int track_height = menu_height; 

    // Сначала точечно затираем старый ползунок дорожкой
    for (int i = 0; i < track_height; i++) {
        mvwaddnwstr(win, track_y_start + i, start_x, L"░", 1); 
    }

    int thumb_height = (track_height * menu_height) / total_items;
    if (thumb_height < 1) thumb_height = 1;

    int max_scroll_index = total_items - 1;
    int max_thumb_pos = track_height - thumb_height;
    
    int thumb_y_offset = 0;
    if (max_scroll_index > 0) {
        thumb_y_offset = (current_index * max_thumb_pos) / max_scroll_index;
    }

    // Рисуем новый ползунок
    wattron(win, COLOR_PAIR(3) | A_BOLD); 
    for (int i = 0; i < thumb_height; i++) {
        mvwaddnwstr(win, track_y_start + thumb_y_offset + i, start_x, L"█", 1);
    }
    wattroff(win, COLOR_PAIR(3) | A_BOLD);
}

void show_database_view() {
    int old_cursor = curs_set(0); // Прячем курсор

    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    int width = 54; 
    int height = 15; 
    int starty = (screen_rows - height) / 2;
    int startx = (screen_cols - width) / 2;

    PANEL *data_panel = create_popup_window(height, width, starty, startx, "БАЗА ДАННЫХ");
    WINDOW *frame_win = panel_window(data_panel);
    
    wbkgd(frame_win, COLOR_PAIR(4));
    wattron(frame_win, COLOR_PAIR(3));

    keypad(frame_win, TRUE);

    std::vector<DbRecord> db_data;
    for (int i = 1; i <= 30; i++) {
        db_data.push_back({
            std::to_wstring(i), 
            L"Игрок_" + std::to_wstring(i), 
            (i % 2 == 0 ? L"Москва" : L"Пермь"), 
            18 + i
        });
    }

    int current_choice = 0; 
    int scroll_offset = 0;  
    int max_visible = 8;    
    bool viewing = true;

    WINDOW *list_win = derwin(frame_win, max_visible, width - 8, 3, 3);

//    idlok(frame_win, FALSE);
//    scrollok(frame_win, FALSE);
//    idlok(list_win, FALSE);
//    scrollok(list_win, FALSE);
    
    // 🌟 СЕКРЕТ СТАБИЛЬНОСТИ: Локальный флаг перерисовки списка
    bool need_redraw = true; 

    // Рисуем неизменяемые элементы один раз при старте
    wattron(frame_win, COLOR_PAIR(4) | A_BOLD);
    mvwaddwstr(frame_win, 2, 3, L"  ID   ИМЯ          ГОРОД        ВОЗРАСТ        ");
    wattroff(frame_win, COLOR_PAIR(4) | A_BOLD);
    mvwprintw(frame_win, height - 2, 3, "Стрелки — Навигация, Enter — Карточка, Q — Выход");

    while (viewing) {
        // 🌟 ОТРИСОВКА СРАБОТАЕТ ТОЛЬКО ПРИ РЕАЛЬНОМ СКРОЛЛЕ ИЛИ СТАРТЕ
        if (need_redraw) {
            for (int i = 0; i < max_visible; i++) {
                int item_idx = scroll_offset + i;
                if (item_idx >= (int)db_data.size()) break;

                const auto& record = db_data[item_idx];
                int row_y = i;

                wchar_t row_buffer[128];
                wchar_t data_part[128];
                
                // 1. Сначала перемещаем курсор на нужную строку (row_y) в самый фланг (0)
                wmove(list_win, row_y, 0);

                // 2. Очищаем эту строку до конца окна, чтобы гарантированно убрать старые следы
                wclrtoeol(list_win);

                // 3. Форматируем и выводим данные в зависимости от того, выбран ли элемент
                if (item_idx == current_choice) {
                    swprintf(data_part, 128, L"> %-4ls %-12ls %-12ls %-3d", 
                             record.id.c_str(), record.name.c_str(), record.city.c_str(), record.age);
                    swprintf(row_buffer, 128, L"%-40ls", data_part);
                    
                    wattron(list_win, COLOR_PAIR(2) | A_BOLD);
                    waddwstr(list_win, row_buffer); // Используем waddwstr вместо mvw, так как курсор УЖЕ на месте
                    wattroff(list_win, COLOR_PAIR(2) | A_BOLD);
                    
                    // Обходной путь для устранения мерцания в NCurses.
                    // Без уникального маркера для каждой строки экрана ncurses распознает прокрутку списка,
                    // как операцию сдвига строки и выбирает оптимизированный путь обновления экрана.
                    // В ncurses 6.5 (Debian 13, xterm-256color) это вызывает периодическое
                    // мерцание черной линии при изменении scroll_offset.
                    // Невидимый маркер делает каждую видимую строку уникальной, предотвращая
                    // оптимизацию и устраняя мерцание.
                    // Проверено экспериментально после обширного тестирования.
                    // Сразу же дописываем невидимый уникальный мусор для ncurses
                    wchar_t invisible_junk[32];
                    swprintf(invisible_junk, 32, L"_%d_%d", item_idx, i);

                    // Включаем текущую цветовую пару окна (например, 4) ПЛЮС атрибут невидимости
                    wattron(list_win, COLOR_PAIR(2) | A_INVIS); 
                    waddwstr(list_win, invisible_junk);
                    // Выключаем оба атрибута
                    wattroff(list_win, COLOR_PAIR(2) | A_INVIS); 
                } else {
                    swprintf(data_part, 128, L"  %-4ls %-12ls %-12ls %-3d", 
                             record.id.c_str(), record.name.c_str(), record.city.c_str(), record.age);
                    swprintf(row_buffer, 128, L"%-40ls", data_part);
                    
                    wattron(list_win, COLOR_PAIR(4));
                    waddwstr(list_win, row_buffer); // Курсор уже спозиционирован шагом №1
                    wattroff(list_win, COLOR_PAIR(4));
                    // Сразу же дописываем невидимый уникальный мусор для ncurses
                    wchar_t invisible_junk[32];
                    swprintf(invisible_junk, 32, L"_%d_%d", item_idx, i);

                    wattron(list_win, COLOR_PAIR(4) | A_INVIS); 
                    waddwstr(list_win, invisible_junk);
                    wattroff(list_win, COLOR_PAIR(4) | A_INVIS); 
                }
            }

            // Обновляем ползунок скроллбара
            draw_scrollbar_thumb(frame_win, max_visible, db_data.size(), current_choice, 48);

            // 1. Говорим ncurses, что под-окно list_win изменилось
            // (так как ncurses не всегда автоматически отслеживает изменения в derwin для панели)
            wnoutrefresh(list_win); 

            // 2. Пересчитываем все панели программы (включая ваше меню и data_panel)
            update_panels(); 

            // 3. Выводим ВСЁ за один кадр на физический экран
            doupdate(); 
            
            // Вывели кадр в память — сбрасываем флаг
            need_redraw = false; 
        }

        int ch = wgetch(frame_win);

        switch (ch) {
            case KEY_UP:
                if (current_choice > 0) {
                    current_choice--;
                    if (current_choice < scroll_offset)
                        scroll_offset = current_choice;
                    need_redraw = true; // 🌟 Нажали кнопку — разрешаем перерисовку строк
                }
                break;
            case KEY_DOWN:
                if (current_choice < (int)db_data.size() - 1) {
                    current_choice++;
                    if (current_choice >= scroll_offset + max_visible)
                        scroll_offset = current_choice - max_visible + 1;
                    need_redraw = true; // 🌟 Нажали кнопку — разрешаем перерисовку строк
                }
                break;
            case 10: { // Enter
                const auto& record = db_data[current_choice];
                
                werase(frame_win);
                box(frame_win, 0, 0); 
                
                mvwprintw(frame_win, 3, 4, "=== ПОДРОБНАЯ КАРТОЧКА ИГРОКА ===");
                mvwprintw(frame_win, 5, 6, "Уникальный ID:  %ls", record.id.c_str());
                mvwprintw(frame_win, 6, 6, "Имя аккаунта:   %ls", record.name.c_str());
                mvwprintw(frame_win, 7, 6, "Родной город:   %ls", record.city.c_str());
                mvwprintw(frame_win, 8, 6, "Полных лет:     %d", record.age);
                mvwprintw(frame_win, 11, 4, "Нажмите Enter для возврата...");
                
                update_panels();
                doupdate();
                
                int card_ch;
                while((card_ch = wgetch(frame_win)) != 10); 
                
                // Возвращаем разметку таблицы при выходе из карточки
                werase(frame_win);
                wattron(frame_win, COLOR_PAIR(3) | A_BOLD);
                box(frame_win, 0, 0);
                mvwprintw(frame_win, 0, (width - 22) / 2, " БАЗА ДАННЫХ ");
                wattron(frame_win, COLOR_PAIR(4) | A_BOLD);
                mvwaddwstr(frame_win, 2, 3, L"  ID   ИМЯ          ГОРОД        ВОЗРАСТ        ");
                wattroff(frame_win, COLOR_PAIR(4) | A_BOLD);
                mvwprintw(frame_win, height - 2, 3, "Стрелки — Навигация, Enter — Карточка, Q — Выход");
                
                need_redraw = true; // 🌟 Вернулись — заставляем строки прорисоваться заново
                break;
            }
            case 'q':
            case 'Q':
            case 27:
                viewing = false;
                break;
        }
    }

    destroy_popup_window(data_panel);
    curs_set(old_cursor); 
    update_panels();
    doupdate();
}

