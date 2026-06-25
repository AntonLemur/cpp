#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <firebird/Interface.h>

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

