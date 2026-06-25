#include "popup.hpp"
#include <ncursesw/ncurses.h>
#include <cstring>
#include <vector>
#include <string>
#include <cstdlib> // Для rand() и srand()
#include <ctime>   // Для инициализации rand()

#define COLOR_POPUP_BOX   3
#define COLOR_POPUP_TEXT  4
#define COLOR_PLAYER      2 // Зеленый игрок
#define COLOR_GOLD        4 // Желтое золото (совпадает с COLOR_POPUP_TEXT, сделаем A_BOLD)
#define COLOR_ENEMY       3 // Красный враг

const int MAP_HEIGHT = 7;
const int MAP_WIDTH = 20;
const char* GAME_MAP[MAP_HEIGHT] = {
    "####################",
    "#......#...........#",
    "#......#...........#",
    "#..........#####...#",
    "#......#.......#...#",
    "#......#.......#...#",
    "####################"
};

// Структура для хранения координат игровых объектов
struct Point {
    int x;
    int y;
};

PANEL* create_popup_window(int height, int width, int starty, int startx, const char *title) {
    WINDOW *win = newwin(height, width, starty, startx);
    PANEL *pan = new_panel(win);
    
    wbkgd(win, COLOR_PAIR(COLOR_POPUP_TEXT));
    wattron(win, COLOR_PAIR(COLOR_POPUP_BOX) | A_BOLD);
    box(win, 0, 0);
    mvwprintw(win, 0, (width - (int)strlen(title)) / 2, " %s ", title);
    wattroff(win, COLOR_PAIR(COLOR_POPUP_BOX) | A_BOLD);
    
    return pan;
}

void destroy_popup_window(PANEL *pan) {
    if (pan) {
        WINDOW *win = panel_window(pan);
        del_panel(pan);
        delwin(win);
    }
}

void show_game_popup() {
    int old_cursor = curs_set(0); 
    srand(time(NULL)); // Инициализируем генератор случайных чисел

    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    int width = 50;
    int height = 15; // Чуть увеличили высоту для вывода счета
    int starty = (screen_rows - height) / 2;
    int startx = (screen_cols - width) / 2;

    PANEL *popup_panel = create_popup_window(height, width, starty, startx, "РЕТРО РОГАЛИК");
    WINDOW *popup_win = panel_window(popup_panel);
    keypad(popup_win, TRUE);

    // Состояние игрока
    int player_x = 1;
    int player_y = 1;
    int score = 0;

    // Состояние врага (Стартует в правом нижнем углу)
    int enemy_x = 18;
    int enemy_y = 5;

    // Генерируем монетки во всех пустых точках карты (где стоит '.')
    std::vector<Point> gold_list;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (GAME_MAP[y][x] == '.' && !(x == player_x && y == player_y)) {
                gold_list.push_back({x, y});
            }
        }
    }
    int total_gold = gold_list.size();

    bool game_running = true;
    std::string status_message = "Соберите всё золото!";

    // 🎮 ИГРОВОЙ ЦИКЛ
    while (game_running) {
        // Очищаем внутреннюю область экрана
        for (int i = 1; i < height - 1; i++) {
            mvwhline(popup_win, i, 1, ' ', width - 2);
        }

        // 1. ОТРИСОВКА КАРТЫ (СТЕНЫ И СВОБОДНЫЕ ЗОНЫ)
        wattron(popup_win, COLOR_PAIR(COLOR_POPUP_TEXT));
        for (int y = 0; y < MAP_HEIGHT; y++) {
            mvwprintw(popup_win, 2 + y, 4, "%s", GAME_MAP[y]);
        }
        wattroff(popup_win, COLOR_PAIR(COLOR_POPUP_TEXT));

        // 2. ОТРИСОВКА МОНЕТОК
        wattron(popup_win, COLOR_PAIR(COLOR_GOLD) | A_BOLD);
        for (const auto& gold : gold_list) {
            mvwaddch(popup_win, 2 + gold.y, 4 + gold.x, '$');
        }
        wattroff(popup_win, COLOR_PAIR(COLOR_GOLD) | A_BOLD);

        // 3. ОТРИСОВКА ВРАГА
        wattron(popup_win, COLOR_PAIR(COLOR_ENEMY) | A_BOLD);
        mvwaddch(popup_win, 2 + enemy_y, 4 + enemy_x, 'E');
        wattroff(popup_win, COLOR_PAIR(COLOR_ENEMY) | A_BOLD);

        // 4. ОТРИСОВКА ИГРОКА
        wattron(popup_win, COLOR_PAIR(COLOR_PLAYER) | A_BOLD);
        mvwaddch(popup_win, 2 + player_y, 4 + player_x, '@');
        wattroff(popup_win, COLOR_PAIR(COLOR_PLAYER) | A_BOLD);

        // 5. ИГРОВОЙ ИНТЕРФЕЙС (Панель состояния)
        wattron(popup_win, COLOR_PAIR(COLOR_POPUP_TEXT) | A_BOLD);
        mvwprintw(popup_win, height - 4, 4, "Золото: %d / %d", score, total_gold);
        mvwprintw(popup_win, height - 3, 4, "Статус: %s", status_message.c_str());
        wattroff(popup_win, COLOR_PAIR(COLOR_POPUP_TEXT) | A_BOLD);
        mvwprintw(popup_win, height - 2, 4, "Стрелки — Ход, Q — Сбежать");

        // Выводим кадр на экран
        top_panel(popup_panel);
        update_panels();
        doupdate();

        // 6. ОБРАБОТКА ВВОДА ИГРОКА
        int ch = wgetch(popup_win);
        int next_px = player_x;
        int next_py = player_y;
        bool player_moved = false;

        switch (ch) {
            case KEY_UP:    next_py--; player_moved = true; break;
            case KEY_DOWN:  next_py++; player_moved = true; break;
            case KEY_LEFT:  next_px--; player_moved = true; break; // Очистим баг декремента
            case KEY_RIGHT: next_px++; player_moved = true; break;
            case 'q':
            case 'Q':
            case 27: 
                game_running = false;
                continue;
        }

        // Проверка коллизии игрока со стенами
        if (GAME_MAP[next_py][next_px] != '#') {
            player_x = next_px;
            player_y = next_py;
        }

        // 7. ЛОГИКА СБОРА ЗОЛОТА
        for (auto it = gold_list.begin(); it != gold_list.end(); ) {
            if (it->x == player_x && it->y == player_y) {
                score++;
                status_message = "Подобрано золото +1!";
                it = gold_list.erase(it); // Удаляем монетку из вектора
            } else {
                ++it;
            }
        }

        // Условие победы
        if (gold_list.empty()) {
            status_message = "ПОБЕДА! Вы собрали всё золото! Нажмите Q.";
        }

        // 8. ХОД ВРАГА (ИИ делает шаг, только если походил игрок)
//        if (player_moved && !gold_list.empty()) {
//            // Генерируем случайное направление (0 - вверх, 1 - вниз, 2 - влево, 3 - вправо)
//            int dir = rand() % 4;
//            int next_ex = enemy_x;
//            int next_ey = enemy_y;

//            switch (dir) {
//                case 0: next_ey--; break;
//                case 1: next_ey++; break;
//                case 2: next_ex--; break;
//                case 3: next_ex++; break;
//            }

//            // Враг может ходить только по свободным клеткам и не может проходить сквозь стены
//            if (GAME_MAP[next_ey][next_ex] != '#') {
//                enemy_x = next_ex;
//                enemy_y = next_ey;
//            }
//        }

        // 8. ХОД ВРАГА (Умное преследование с балансом сложности)
        if (player_moved && !gold_list.empty()) {
            int next_ex = enemy_x;
            int next_ey = enemy_y;

            // Вычисляем расстояние до игрока с помощью простой геометрии (Манхэттенское расстояние)
            int distance = std::abs(enemy_x - player_x) + std::abs(enemy_y - player_y);

            // Если игрок близко (ближе 6 клеток) и сработал шанс 60% на "умный" ход
            if (distance <= 6 && (rand() % 100 < 60)) {
                status_message = "Враг заметил вас и преследует!";
                
                // Выбираем, по какой оси сокращать дистанцию
                // Если разница по X больше, двигаемся по горизонтали
                if (std::abs(enemy_x - player_x) > std::abs(enemy_y - player_y)) {
                    if (enemy_x > player_x) next_ex--;
                    else next_ex++;
                } 
                // Иначе сокращаем дистанцию по вертикали
                else {
                    if (enemy_y > player_y) next_ey--;
                    else next_ey++;
                }
            } 
            // Если игрок далеко или враг "затупил" (40% шанс) — просто бродим случайным образом
            else {
                int dir = rand() % 4;
                switch (dir) {
                    case 0: next_ey--; break;
                    case 1: next_ey++; break;
                    case 2: next_ex--; break;
                    case 3: next_ex++; break;
                }
            }

            // Физика врага: проверяем, чтобы он не ходил сквозь стены
            // Если умный ход уперся в стену, враг просто останется на месте (что тоже на руку игроку)
            if (GAME_MAP[next_ey][next_ex] != '#') {
                enemy_x = next_ex;
                enemy_y = next_ey;
            }
        }

        // Условие проигрыша: коллизия игрока и врага
        if (player_x == enemy_x && player_y == enemy_y) {
            status_message = "ВРАГ ПОЙМАЛ ВАС! Игра перезапущена!";
            // Сбрасываем позиции
            player_x = 1; player_y = 1;
            enemy_x = 18; enemy_y = 5;
            
            // Перерисовываем монетки заново
            gold_list.clear();
            for (int y = 0; y < MAP_HEIGHT; y++) {
                for (int x = 0; x < MAP_WIDTH; x++) {
                    if (GAME_MAP[y][x] == '.' && !(x == player_x && y == player_y)) {
                        gold_list.push_back({x, y});
                    }
                }
            }
            score = 0;
            
            update_panels();
            doupdate();
            wgetch(popup_win); // Пауза, чтобы игрок успел прочитать сообщение
        }
    }

    destroy_popup_window(popup_panel);
    curs_set(old_cursor);
    update_panels();
    doupdate();
}

