#include <locale.h>
#include <ncursesw/ncurses.h>
#include <ncursesw/panel.h>
#include "popup.hpp"
#include "settings.hpp"
#include "dataview.hpp"
#include "db_manager.h"
#include <iostream>

#define COLOR_MENU_BG     1
#define COLOR_MENU_ACTIVE 2
#define COLOR_POPUP_BOX   3
#define COLOR_POPUP_TEXT  4

int main() {
    setlocale(LC_ALL, "");
    
    // Инициализация базы данных ДО старта графики ncurses
    if (!DBManager::connect()) {
        std::cerr << "Критическая ошибка: Не удалось подключиться к базе данных Firebird!\n";
        return 1;
    }

    initscr();             
    cbreak();                
    noecho();                
    keypad(stdscr, TRUE);
    curs_set(0); // Скрываем курсор
    
    // При старте программы или перед открытием меню:
    idlok(stdscr, FALSE);
    scrollok(stdscr, FALSE);    

    if (has_colors() == FALSE) {
        endwin();
        printf("Ваш терминал не поддерживает цвета!\n");
        return 1;
    }
    
    start_color();
    init_pair(COLOR_MENU_BG,     COLOR_WHITE,   COLOR_BLACK);  
    init_pair(COLOR_MENU_ACTIVE, COLOR_BLACK,   COLOR_GREEN);  
    init_pair(COLOR_POPUP_BOX,   COLOR_RED,     COLOR_BLUE);   
    init_pair(COLOR_POPUP_TEXT,  COLOR_YELLOW,  COLOR_BLUE);
    
    // 🌟 1. АКТИВИРУЕМ МЫШЬ В NCURSES
    // Передаем маску событий: ALL_MOUSE_EVENTS включает отслеживание кликов.
    // На некоторых терминалах также полезно REPORT_MOUSE_POSITION для трекинга.
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    
    // Включаем генерацию щелчков без задержек (по умолчанию ncurses ждет двойного клика)
    mouseinterval(0); 

    PANEL *base_panel = new_panel(stdscr);
    bkgd(COLOR_PAIR(COLOR_MENU_BG));

    const char *menu[] = {"1. Начать игру", "2. Настройки", "3. Данные из БД", "4. Выход"};
    int choice = 0;
    int key;
    bool need_redraw = true; // 🌟 Флаг, управляющий отрисовкой

    while(1) {
        if (need_redraw) {
            // 🌟 ИСПРАВЛЕНИЕ: Заменили clear() на erase()
            erase(); 
 
            attron(COLOR_PAIR(COLOR_MENU_BG));
            mvprintw(1, 2, "Используйте стрелки/мышь для выбора и Enter/Клик:");
            attroff(COLOR_PAIR(COLOR_MENU_BG));

            for(int i = 0; i < 4; i++) {
                if(i == choice) {
                    attron(COLOR_PAIR(COLOR_MENU_ACTIVE) | A_BOLD);
                    mvprintw(3 + i, 4, "  %s  ", menu[i]);
                    attroff(COLOR_PAIR(COLOR_MENU_ACTIVE) | A_BOLD);
                } else {
                    attron(COLOR_PAIR(COLOR_MENU_BG));
                    mvprintw(3 + i, 4, "  %s  ", menu[i]);
                    attroff(COLOR_PAIR(COLOR_MENU_BG));
                }
            }
         
            need_redraw=false;
        }
        
//        update_panels();
//        doupdate();

        key = getch();
        
        // 🌟 2. ПЕРЕХВАТ СООБЩЕНИЯ МЫШИ
        if (key == KEY_MOUSE) {
            MEVENT mouse_event;
            // Извлекаем данные о событии мыши в структуру
            if (getmouse(&mouse_event) == OK) {
                // Проверяем, что была нажата (или отпущена) левая кнопка мыши
                if (mouse_event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_RELEASED)) {
                    
                    // Проверяем, попал ли клик по области нашего меню по оси X (колонки)
                    // Длина строк меню с пробелами примерно от X=4 до X=22
                    if (mouse_event.x >= 4 && mouse_event.x <= 22) {
                        
                        // Проверяем попадание по оси Y (строки 3, 4, 5)
                        if (mouse_event.y >= 3 && mouse_event.y <= 6) {
                            int clicked_choice = mouse_event.y - 3; // Вычисляем индекс пункта (0, 1 или 2)
                            
                            if (clicked_choice == choice) {
                                // Если кликнули по УЖЕ активному пункту — это аналог нажатия Enter!
                                key = 10; 
                            } else {
                                // Если кликнули по другому пункту — просто переводим туда фокус подсветки
                                choice = clicked_choice;
                                need_redraw=true;
                                continue;
                            }
                        }
                    }
                }
            }
        }        
        
        // Клавиатурная навигация
        if(key == KEY_UP && choice > 0) {
            choice--;
            need_redraw = true; // Сдвинули стрелку — надо перерисовать
        }
        if(key == KEY_DOWN && choice < 3) {
            choice++;
            need_redraw = true; // Сдвинули стрелку — надо перерисовать
        }
        
        if(key == 10) { 
            if (choice == 0) {
                show_game_popup(); 
                need_redraw = true;
            } else if (choice == 1) {
                show_settings(); // Теперь настройки красиво всплывают как модуль!
                need_redraw = true;
            } else if (choice == 2) {
                show_database_view(); //🌟 Открываем список БД
                need_redraw = true;
            } else if (choice == 3) {
                break; 
            }
        }
    }

    del_panel(base_panel);
    endwin();
    
    // Закрытие соединения с базой данных
    DBManager::disconnect();
     
    return 0;
}

