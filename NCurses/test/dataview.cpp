#define _XOPEN_SOURCE_EXTENDED 1
#define NCURSES_WIDECHAR 1

#include "dataview.hpp"
#include "popup.hpp"
#include <ncursesw/ncurses.h>
#include <ncursesw/menu.h>
#include <vector>
#include <string>

// Полноценная запись из БД с кучей полей
struct DbRecord {
    std::string id;
    std::string name;
    std::string city;
    int age;
};


void show_database_view() {
    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    // Размеры внешнего окна-панели
    int width = 50;
    int height = 14; 
    int starty = (screen_rows - height) / 2;
    int startx = (screen_cols - width) / 2;

    PANEL *data_panel = create_popup_window(height, width, starty, startx, "БАЗА ДАННЫХ");
    WINDOW *data_win = panel_window(data_panel);
    keypad(data_win, TRUE);

    // Имитируем сложную выборку из БД (4 поля)
    std::vector<DbRecord> db_data = {
        {"1", "Антон", "Челябинск", 25},
        {"2", "Елена", "Москва",     30},
        {"3", "Иван",  "Новосибирск",22}
    };

    // 2. Создаем элементы меню для Ncurses (массив ITEM*)
    // Ncurses требует, чтобы массив заканчивался NULL
    std::vector<ITEM*> menu_items;
    for (size_t i = 0; i < db_data.size(); i++) {
        // Для внешнего вида используем только ID и Имя
        ITEM* item = new_item(db_data[i].id.c_str(), db_data[i].name.c_str());
        
        // 🌟 СЕКРЕТ: Привязываем указатель на нашу полную структуру из вектора
        set_item_userptr(item, (void*)&db_data[i]);
        
        menu_items.push_back(item);
    }
    menu_items.push_back(NULL);

    // 3. Инициализируем меню
    MENU *my_menu = new_menu(menu_items.data());

    // 4. Создаем "под-окно" (Subwindow) внутри нашей панели специально для списка
    // Высота списка 8 строк, ширина 42, отступы Y=3, X=4 относительно панели
    WINDOW *menu_win = derwin(data_win, 8, 42, 3, 4);
    keypad(menu_win, TRUE);

    // Связываем меню с окнами
    set_menu_win(my_menu, data_win);
    set_menu_sub(my_menu, menu_win);

    // Настраиваем формат отображения: показывать одновременно 6 строк в 1 колоку
    // Если элементов больше 6, автоматически включится прокрутка (скролл)!
    set_menu_format(my_menu, 6, 1);
    
    // Задаем маркер активной строки
    set_menu_mark(my_menu, " > ");

    // Выводим меню на экран
    post_menu(my_menu);
    
    // Подсказка внизу окна
    mvwprintw(data_win, height - 2, 4, "Стрелки — Скролл, Enter — Выбрать, Q — Выход");

    update_panels();
    doupdate();

    int ch;
    bool viewing = true;

    // 5. Цикл прокрутки и обработки событий
    while (viewing) {
        ch = wgetch(data_win);

        switch (ch) {
            case KEY_DOWN:
                menu_driver(my_menu, REQ_DOWN_ITEM); // Скролл вниз
                break;
            case KEY_UP:
                menu_driver(my_menu, REQ_UP_ITEM);   // Скролл вверх
                break;
            case KEY_NPAGE:
                menu_driver(my_menu, REQ_SCR_DPAGE); // Страница вниз (PageDown)
                break;
            case KEY_PPAGE:
                menu_driver(my_menu, REQ_SCR_UPAGE); // Страница вверх (PageUp)
                break;
            case 10: { // Enter — выбор элемента
                ITEM *cur = current_item(my_menu);
                // 🌟 Извлекаем наш скрытый указатель обратно
                DbRecord *selected_record = (DbRecord*)item_userptr(cur);
                
                if (selected_record) {
                    // Теперь у нас есть прямой доступ ко всем оригинальным полям БД!
                    wclear(data_win); // Очистим окно панели для вывода карточки
                    box(data_win, 0, 0); // Вернем рамку
                    
                    // Красиво выводим ВСЕ 4 поля в окне
                    mvwprintw(data_win, 3, 4, "=== КАРТОЧКА ИЗ БД ===");
                    mvwprintw(data_win, 5, 4, "ID:      %s", selected_record->id.c_str());
                    mvwprintw(data_win, 6, 4, "Имя:     %s", selected_record->name.c_str());
                    mvwprintw(data_win, 7, 4, "Город:   %s", selected_record->city.c_str());
                    mvwprintw(data_win, 8, 4, "Возраст: %d", selected_record->age);
                    mvwprintw(data_win, 11, 4, "Нажмите любую клавишу для возврата...");
                    
                    wrefresh(data_win);
                    wgetch(data_win); // Ждем нажатия
                    
                    // Возвращаем меню обратно на экран
                    wclear(data_win);
                    box(data_win, 0, 0);
                    mvwprintw(data_win, 0, (width - 22) / 2, " БАЗА ДАННЫХ "); // Возвращаем заголовок окна
                    mvwprintw(data_win, height - 2, 4, "Стрелки — Скролл, Enter — Выбрать, Q — Выход");
                    
                    // 🌟 РЕШЕНИЕ: Полный перезапуск рендеринга меню
                    // 1. Снимаем меню с публикации (это сбрасывает все внутренние маски отрисовки)
                    unpost_menu(my_menu);
                    
                    // 2. Публикуем меню заново на чистый холст окна
                    post_menu(my_menu); 
                    
                    // 3. Синхронизируем положение курсора
                    pos_menu_cursor(my_menu); 
                    
                    // 4. Окончательно обновляем окна на экране
                    wrefresh(data_win);
                    wrefresh(menu_win);
                }
                
                break;
            }
            case 'q':
            case 'Q':
            case 27: // Escape (на некоторых терминалах требует двойного нажатия)
                viewing = false;
                break;
        }

        // Перерисовываем изменения внутри окон меню
        wrefresh(data_win);
    }

    // 6. Очистка памяти
    unpost_menu(my_menu);
    free_menu(my_menu);
    for (size_t i = 0; i < menu_items.size() - 1; i++) {
        free_item(menu_items[i]);
    }
    delwin(menu_win);
    destroy_popup_window(data_panel);

    update_panels();
    doupdate();
}

