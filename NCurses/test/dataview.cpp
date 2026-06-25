#define _XOPEN_SOURCE_EXTENDED 1
#define NCURSES_WIDECHAR 1

#include "dataview.hpp"
#include "popup.hpp"
#include <ncursesw/ncurses.h>
#include <ncursesw/panel.h>
#include <vector>
#include <string>
#include <cwchar>
#include "employee_service.h" // Всё, что нужно для данных — теперь здесь!
#include "photo_viewer.h"

// Задаем константы геометрии скроллбара
const int TRACK_Y_START = 3;
const int TRACK_HEIGHT = 8;
const int SCROLLBAR_X = 86;

// Оптимизированная функция: рисует ТОЛЬКО ползунок, не трогая дорожку
void draw_scrollbar_thumb(WINDOW *win, int menu_height, int total_items, int current_index, int start_x) {
    if (total_items <= menu_height) return; 

    // Сначала точечно затираем старый ползунок дорожкой
    for (int i = 0; i < TRACK_HEIGHT; i++) {
        mvwaddnwstr(win, TRACK_Y_START + i, start_x, L"░", 1); 
    }

    int thumb_height = (TRACK_HEIGHT * menu_height) / total_items;
    if (thumb_height < 1) thumb_height = 1;

    int max_scroll_index = total_items - 1;
    int max_thumb_pos = TRACK_HEIGHT - thumb_height;
    
    int thumb_y_offset = 0;
    if (max_scroll_index > 0) {
        thumb_y_offset = (current_index * max_thumb_pos) / max_scroll_index;
    }

    // Рисуем новый ползунок
    wattron(win, COLOR_PAIR(3) | A_BOLD); 
    for (int i = 0; i < thumb_height; i++) {
        mvwaddnwstr(win, TRACK_Y_START + thumb_y_offset + i, start_x, L"█", 1);
    }
    wattroff(win, COLOR_PAIR(3) | A_BOLD);
}

void show_employee_card_window(WINDOW* data_win, int current_employee_id) {
    // 1. Загружаем из базы ВСЕ данные сотрудника (включая BLOB фото) через наш сервис
    Employee current_emp = get_employee_details(current_employee_id);

    bool in_card = true;
    while (in_card) {
        // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ 1: Полностью очищаем окно перед каждым рендером,
        // чтобы стереть остатки старого списка сотрудников под карточкой
        werase(data_win);
        
        // Рисуем рамку вокруг карточки (опционально, для красоты)
        box(data_win, 0, 0);

        // 2. Отрисовываем текстовые поля в TUI...
        mvwprintw(data_win, 2, 4, "=== ПОДРОБНАЯ КАРТОЧКА ИГРОКА ===");
        mvwprintw(data_win, 4, 6, "Уникальный ID:  %d", current_emp.id);
        mvwprintw(data_win, 5, 6, "SN:           %lld", current_emp.sn);
        mvwprintw(data_win, 6, 6, "SN_INT:       %lld", current_emp.si);
        
        // Передаем .c_str(), так как ncurses ожидает указатель на массив wchar_t для флага %ls
        mvwprintw(data_win, 7, 6, "Фамилия:      %ls", current_emp.lastName.c_str());
        mvwprintw(data_win, 8, 6, "Имя:          %ls", current_emp.firstName.c_str());
        
        // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ 2: Сместили отчество на строку 8 (была опечатка с 8 строкой для имени)
        mvwprintw(data_win, 9, 6, "Отчество:     %ls", current_emp.middleName.c_str());
        
        // Показываем подсказку 'P' только если у сотрудника физически есть фото в базе
        if (current_emp.hasPhoto) {
            mvwprintw(data_win, 11, 4, "[Нажмите 'P' для просмотра фото сотрудника]");
        } else {
            wattron(data_win, A_DIM);
            mvwprintw(data_win, 11, 4, "[Фотография сотрудника отсутствует в базе]");
            wattroff(data_win, A_DIM);
        }
        
        mvwprintw(data_win, 13, 4, "Нажмите Enter для возврата к списку...");
        wrefresh(data_win);

        // 3. КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ 3: Интерактивный цикл ввода
        int ch = wgetch(data_win);
        
        if (ch == 'p' || ch == 'P') {
            if (current_emp.hasPhoto) {
                // Передаем готовую структуру в модуль отображения
                view_employee_photo(current_emp);
                
                // fim затер терминал. Говорим ncurses принудительно перерисовать весь физический экран
                clearok(curscr, TRUE);
            }
        } 
        // 10 — это код клавиши Enter. При нажатии выходим из цикла карточки
        else if (ch == 10) {
            in_card = false;
        }
    }
    
    // Перед возвратом в главное окно списка очищаем за собой экран карточки,
    // чтобы при возврате список заново отрисовался на чистой поверхности
    update_panels();
    doupdate();
}

void show_database_view() {
    int old_cursor = curs_set(0); // Прячем курсор

    // 🌟 Включаем расширенное отслеживание мыши, включая перемещение с зажатой кнопкой 1
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    int width = 90; 
    int height = 15; 
    int starty = (screen_rows - height) / 2;
    int startx = (screen_cols - width) / 2;

    PANEL *data_panel = create_popup_window(height, width, starty, startx, "БАЗА ДАННЫХ");
    WINDOW *frame_win = panel_window(data_panel);
    
    wbkgd(frame_win, COLOR_PAIR(4));
    wattron(frame_win, COLOR_PAIR(3));

    keypad(frame_win, TRUE);

    // Получаем легкий список одной строчкой кода без SQL внутри интерфейса!
    std::vector<Employee> db_data = get_employees_list(); 

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
    mvwaddwstr(frame_win, 2, 3, L"  ID   SN                   SN_INT               ФИО                  Фото");
    wattroff(frame_win, COLOR_PAIR(4) | A_BOLD);
    mvwprintw(frame_win, height - 2, 3, "Стрелки — Навигация, Enter — Карточка, Q — Выход");
//    mvwprintw(frame_win, height - 1, 3, "Кол-во записей: %d", db_data.size());

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
                
                // Формируем красивое ФИО: Иванов И. И.
                std::wstring short_fn = record.firstName.empty() ? L"" : record.firstName.substr(0, 1) + L".";
                std::wstring short_mn = record.middleName.empty() ? L"" : record.middleName.substr(0, 1) + L".";
                std::wstring full_name = record.lastName + L" " + short_fn + short_mn;

                // Метка наличия фото
                std::wstring photo_marker = record.hasPhoto ? L"[P]" : L"   ";

                // 3. Форматируем и выводим данные в зависимости от того, выбран ли элемент
                if (item_idx == current_choice) {
                    // Префикс "> " для выбранного элемента
                    swprintf(data_part, 128, L"> %-4d %-20lld %-20lld %-20ls %-3ls", 
                             record.id, record.sn, record.si, full_name.c_str(), photo_marker.c_str());
                    swprintf(row_buffer, 128, L"%-74ls", data_part); // Увеличили ширину строки под новые данные
                    
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
                    // Обычная строка списка
                    swprintf(data_part, 128, L"  %-4d %-20lld %-20lld %-20ls %-3ls", 
                             record.id, record.sn, record.si, full_name.c_str(), photo_marker.c_str());
                    swprintf(row_buffer, 128, L"%-74ls", data_part);
                    
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
            draw_scrollbar_thumb(frame_win, max_visible, db_data.size(), current_choice, SCROLLBAR_X);

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
                
            // 🌟 ОБРАБОТКА МЫШИ (Исправленная математика и поддержка колесика)
            case KEY_MOUSE: {
                MEVENT mouse_event;
                if (getmouse(&mouse_event) == OK) {
                    int local_y = mouse_event.y - starty;
                    int local_x = mouse_event.x - startx;

                    // 1. ПОДДЕРЖКА КОЛЕСИКА МЫШИ (Работает в любом месте окна!)
                    // В ncurses скролл колесиком часто мапится на BUTTON4 (вверх) и BUTTON5 (вниз)
                    // или на специальные биты расширенных событий (в зависимости от терминала)
                    #if defined(NCURSES_MOUSE_VERSION) && NCURSES_MOUSE_VERSION >= 2
                    if (mouse_event.bstate & 0x00010000) { // Скролл вверх (обычно маска для колесика вверх)
                        if (current_choice > 0) {
                            current_choice--;
                            if (current_choice < scroll_offset) scroll_offset = current_choice;
                            need_redraw = true; // 🌟 Разрешаем перерисовку строк
                        }
                    }
                    if (mouse_event.bstate & 0x00200000) { // Скролл вниз (обычно маска для колесика вниз)
                        if (current_choice < (int)db_data.size() - 1) {
                            current_choice++;
                            if (current_choice >= scroll_offset + max_visible) {
                                scroll_offset = current_choice - max_visible + 1;
                            }
                            need_redraw = true; // 🌟 Разрешаем перерисовку строк
                        }
                    }
                    #endif

                    // 2. КЛИК ПО СКРОЛЛБАРУ (Исправленная точная математика)
                    if (local_x == SCROLLBAR_X) {
                        if (local_y >= TRACK_Y_START && local_y < TRACK_Y_START + TRACK_HEIGHT) {
                            
                            int click_y = local_y - TRACK_Y_START;
                            int total_items = (int)db_data.size();

                            // Исправленная формула с плавающей точкой для идеальной точности.
                            // Мы делим на (TRACK_HEIGHT - 1), чтобы клик по самой нижней ячейке 
                            // давал ровно 1.0, что при умножении гарантированно вернет последний индекс.
                            double ratio = (double)click_y / (double)(TRACK_HEIGHT - 1);
                            int calculated_choice = (int)(ratio * (total_items - 1));

                            // Жесткая проверка границ (не даст промахнуться мимо массива)
                            if (calculated_choice < 0) calculated_choice = 0;
                            if (calculated_choice >= total_items) calculated_choice = total_items - 1;

                            current_choice = calculated_choice;

                            // Плавное выравнивание списка относительно выбранного элемента
                            scroll_offset = current_choice - (max_visible / 2);
                            if (scroll_offset < 0) scroll_offset = 0;
                            if (scroll_offset > total_items - max_visible) {
                                scroll_offset = total_items - max_visible;
                            }
                            need_redraw = true; // 🌟 Разрешаем перерисовку строк
                        }
                    }
                }
                break;
            }
                
            case 10: { //🌟 Enter — Показываем подробную карточку + ФОТО
                const auto& record = db_data[current_choice];
                
                // 1. Открываем карточку (она работает в том же frame_win)
                show_employee_card_window(frame_win, record.id);                
                
                // 2. Взводим флаг перерисовки СТРОК списка
                need_redraw = true;

                // 3. Возвращаем разметку таблицы при выходе из карточки
                werase(frame_win);
                wattron(frame_win, COLOR_PAIR(3) | A_BOLD);
                box(frame_win, 0, 0);
                mvwprintw(frame_win, 0, (width - 22) / 2, " БАЗА ДАННЫХ ");
                wattron(frame_win, COLOR_PAIR(4) | A_BOLD);
                mvwaddwstr(frame_win, 2, 3, L"  ID   SN                   SN_INT               ФИО                  Фото");
                wattroff(frame_win, COLOR_PAIR(4) | A_BOLD);
                mvwprintw(frame_win, height - 2, 3, "Стрелки — Навигация, Enter — Карточка, Q — Выход");

                // 4. РЕШЕНИЕ: Говорим подсистеме панелей, что порядок слоев изменился 
                // и нужно обновить виртуальный экран для всех окон
                update_panels();
                                 
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
    // Возвращаем стандартную маску мыши (без трекинга зажатия), чтобы не спамить главное меню
    mousemask(ALL_MOUSE_EVENTS, NULL);
    curs_set(old_cursor); 
    update_panels();
    doupdate();
}

