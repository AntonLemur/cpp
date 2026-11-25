#ifndef EMPLOYEE_SERVICE_H
#define EMPLOYEE_SERVICE_H

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include "db_manager.h"

template <typename T>
struct Result{
    bool isOk = false;
    std::string last_db_error;
    T value;
};

// Единая структура сотрудника для всего приложения
struct Employee {
    int id = 0;                          // 0 = создание нового, > 0 = обновление
    std::optional<long long> sn;         // SN (BIGINT)
    long long si = 0;                    // Ваше поле SI

    std::string firstName;               // FIRST_NAME VARCHAR(64)
    std::string middleName;              // MIDDLE_NAME VARCHAR(64)
    std::string lastName;                // LAST_NAME VARCHAR(64)

    std::optional<std::string> carNumber;    // CAR_NUMBER VARCHAR(32)
    std::optional<short> permissions;        // PERMISSIONS SMALLINT
    std::optional<ISC_TIME> workBegin;       // WORK_BEGIN TIME
    std::optional<ISC_TIME> workEnd;         // WORK_END TIME
    int allowedWork5p = 0;                   // ALLOWED_WORK_5P INTEGER = 0
    std::optional<short> category;           // CATEGORY SMALLINT = null

    std::vector<uint8_t> photo;          // Полный массив байт фото
    bool hasPhoto = false;
};

// Возвращает вектор "легких" сотрудников для отображения в таблице/списке (без загрузки самих BLOB в память)
std::vector<Employee> get_employees_list();

// Возвращает одного "полного" сотрудника по его ID (включая загрузку тяжелого BLOB фото)
Employee get_employee_details(int employee_id);

Result<int> update_employee_name(
    int id,
    const std::string& last_name,
    const std::string& first_name,
    const std::string& middle_name);

Result<int> update_employee_photo(int id, const std::vector<uint8_t>& photo_bytes);


Result<int> update_employee(
    int id,
    const std::string& last_name,
    const std::string& first_name,
    const std::string& middle_name);

Result<Employee> get_employee_details_new(int employee_id);

#endif // EMPLOYEE_SERVICE_H

