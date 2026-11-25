#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <ibase.h>
#include <firebird/Interface.h>
// Это известный баг в самом заголовочном файле Firebird 4.0 (/usr/include/firebird/Message.h):
// в нём разработчики Firebird по ошибке написали SQL_TIME вместо стандартной константы SQL_TYPE_TIME из <ibase.h>.
#ifndef SQL_TIME
#define SQL_TIME SQL_TYPE_TIME
#endif
#include <firebird/Message.h>
#include <string>
#include <optional>
#include <cstdio>

using namespace Firebird;

// Для любых числовых типов (long long, short, int)
template <typename T>
std::string optToString(const std::optional<T>& opt, const std::string& fallback = "—") {
    return opt.has_value() ? std::to_string(*opt) : fallback;
}

// Перегрузка для std::optional<std::string> (например, carNumber)
inline std::string optToString(const std::optional<std::string>& opt, const std::string& fallback = "—") {
    return opt.value_or(fallback);
}

// Для времени Firebird (ISC_TIME -> "HH:MM" или "HH:MM:SS")
inline std::string optTimeToString(const std::optional<ISC_TIME>& opt,
                                   Firebird::IUtil* util,
                                   const std::string& fallback = "—") {
    if (!opt.has_value()) {
        return fallback;
    }
    unsigned int hours = 0, minutes = 0, seconds = 0, fractions = 0;
    util->decodeTime(*opt, &hours, &minutes, &seconds, &fractions);

    char buf[16]; // Формат "HH:MM" (или "%02u:%02u:%02u" с буфером char buf)
    std::snprintf(buf, sizeof(buf), "%02u:%02u", hours, minutes);
    return std::string(buf);
}

class DBManager {
public:
    // Инициализация подключения (вызывается один раз в main)
    static bool connect();
    
    // Закрытие подключения (вызывается перед выходом из main)
    static void disconnect();

    // Методы для получения активных указателей в любом месте программы
    static Firebird::IAttachment* getAttachment();
    static Firebird::IMaster* getMaster();
    static Firebird::IStatus* getStatus();

private:
    DBManager() = default; // Запрещаем создание экземпляров класса

    static Firebird::IMaster* master;
    static Firebird::IStatus* status;
    static Firebird::IProvider* provider;
    static Firebird::IXpbBuilder* dpb;
    static Firebird::IAttachment* att;
};

#endif // DB_MANAGER_H

