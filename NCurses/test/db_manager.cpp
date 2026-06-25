#include "db_manager.h"
#include <iostream>

// 1. Объявление статических переменных (ОБЯЗАТЕЛЬНО БЕЗ ИЗМЕНЕНИЙ)
Firebird::IMaster* DBManager::master = nullptr;
Firebird::IStatus* DBManager::status = nullptr;
Firebird::IProvider* DBManager::provider = nullptr;
Firebird::IXpbBuilder* DBManager::dpb = nullptr;
Firebird::IAttachment* DBManager::att = nullptr;

bool DBManager::connect() {
    try {
        master = Firebird::fb_get_master_interface();
        status = master->getStatus();
        provider = master->getDispatcher();

        Firebird::IUtil* utl = master->getUtilInterface();
        Firebird::ThrowStatusWrapper wrappedStatus(status);

        dpb = utl->getXpbBuilder(&wrappedStatus, Firebird::IXpbBuilder::DPB, nullptr, 0);
        
        dpb->insertString(&wrappedStatus, isc_dpb_user_name, "SYSDBA");
        dpb->insertString(&wrappedStatus, isc_dpb_password, "56911317"); // ваш пароль
        dpb->insertString(&wrappedStatus, isc_dpb_lc_ctype, "UTF8");

        // ВНИМАНИЕ: Здесь НЕ должно быть префикса "Firebird::IAttachment*". 
        // Мы пишем строго в статическую переменную класса:
        att = provider->attachDatabase(&wrappedStatus, "localhost:teko", 
                                      dpb->getBufferLength(&wrappedStatus), dpb->getBuffer(&wrappedStatus));

        if (status->getState() & Firebird::IStatus::STATE_ERRORS) {
            return false; 
        }
        
        // Добавим отладку прямо в момент подключения для проверки инициализации
        std::cout << "[ОТЛАДКА СИНГЛТОНА] Подключено! Указатель att: " << att << ", status: " << status << "\n";
        
        return true;
    } catch (...) {
        return false;
    }
}

void DBManager::disconnect() {
    Firebird::ThrowStatusWrapper wrappedStatus(status);
    try {
        if (att) att->detach(&wrappedStatus);
        if (dpb) dpb->dispose(); 
    } catch (...) {}
    if (status) status->dispose();
}

// Убедитесь, что геттеры возвращают именно эти статические переменные
Firebird::IAttachment* DBManager::getAttachment() { return att; }
Firebird::IMaster* DBManager::getMaster() { return master; }
Firebird::IStatus* DBManager::getStatus() { return status; }

