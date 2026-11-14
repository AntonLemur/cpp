#ifndef PHOTO_VIEWER_H
#define PHOTO_VIEWER_H

#include "employee_service.h" // Заголовок, где объявлена структура Employee

// Открывает фотографию сотрудника во внешнем просмотрщике в полном разрешении.
// Перед вызовом из FTXUI необходимо обернуть в screen.WithRestoredIO([&] { ... }).
void view_employee_photo_fullscreen(const Employee& emp);

#endif // PHOTO_VIEWER_H
