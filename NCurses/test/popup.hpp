#pragma once
#include <ncursesw/panel.h>

// Создает цветное всплывающее окно с рамкой и оборачивает его в панель
PANEL* create_popup_window(int height, int width, int starty, int startx, const char *title);

// Безопасно удаляет панель и привязанное к ней окно
void destroy_popup_window(PANEL *pan);

// Демонстрационное игровое окно
void show_game_popup();

