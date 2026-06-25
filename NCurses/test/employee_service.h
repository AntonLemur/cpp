#ifndef EMPLOYEE_SERVICE_H
#define EMPLOYEE_SERVICE_H

#include <string>
#include <vector>
#include <cstdint>

// Единая структура сотрудника для всего приложения
struct Employee {
    int id;
    long long sn;
    long long si;
    std::wstring firstName;
    std::wstring middleName;
    std::wstring lastName;
    std::vector<uint8_t> photo; // Полный массив байт фото (используется в карточке)
    bool hasPhoto;              // Флаг наличия фото (используется в списке)
};

// Возвращает вектор "легких" сотрудников для отображения в таблице/списке (без загрузки самих BLOB в память)
std::vector<Employee> get_employees_list();

// Возвращает одного "полного" сотрудника по его ID (включая загрузку тяжелого BLOB фото)
Employee get_employee_details(int employee_id);

#endif // EMPLOYEE_SERVICE_H

