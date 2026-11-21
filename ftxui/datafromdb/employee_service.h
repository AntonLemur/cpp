#ifndef EMPLOYEE_SERVICE_H
#define EMPLOYEE_SERVICE_H

#include <string>
#include <vector>
#include <cstdint>

struct Result{
    bool isOk = false;
    std::string last_db_error;
};

// Единая структура сотрудника для всего приложения
struct Employee {
    int id;
    long long sn;
    long long si;
    std::string firstName;
    std::string middleName;
    std::string lastName;
    std::vector<uint8_t> photo; // Полный массив байт фото (используется в карточке)
    bool hasPhoto;              // Флаг наличия фото (используется в списке)
};

// Возвращает вектор "легких" сотрудников для отображения в таблице/списке (без загрузки самих BLOB в память)
std::vector<Employee> get_employees_list();

// Возвращает одного "полного" сотрудника по его ID (включая загрузку тяжелого BLOB фото)
Employee get_employee_details(int employee_id);

Result update_employee_name(
    int id,
    const std::string& last_name,
    const std::string& first_name,
    const std::string& middle_name);

Result update_employee_photo(int id, const std::vector<uint8_t>& photo_bytes);

#endif // EMPLOYEE_SERVICE_H

