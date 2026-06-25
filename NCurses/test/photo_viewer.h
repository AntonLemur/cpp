#ifndef PHOTO_VIEWER_H
#define PHOTO_VIEWER_H

#include "employee_service.h" // Для доступа к структуре Employee

// Функция берет уже скачанные байты фото, сохраняет их на RAM-диск,
// гасит ncurses, отображает картинку через fim и возвращает TUI назад.
void view_employee_photo(const Employee& emp);

#endif // PHOTO_VIEWER_H

