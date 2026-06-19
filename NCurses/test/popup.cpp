#include "popup.hpp"
#include <ncursesw/ncurses.h>
#include <cstring>

#define COLOR_POPUP_BOX   3
#define COLOR_POPUP_TEXT  4

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
    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    int width = 44;
    int height = 10;
    int starty = (screen_rows - height) / 2;
    int startx = (screen_cols - width) / 2;

    PANEL *popup_panel = create_popup_window(height, width, starty, startx, "ВНИМАНИЕ");
    WINDOW *popup_win = panel_window(popup_panel);

    mvwprintw(popup_win, 3, 4, "Вы запустили тестовое окно!");
    mvwprintw(popup_win, 5, 4, "Код теперь разбит на модули.");
    mvwprintw(popup_win, 7, 4, "Нажмите любую клавишу...");

    update_panels();
    doupdate();

    getch();

    destroy_popup_window(popup_panel);
    update_panels();
    doupdate();
}

