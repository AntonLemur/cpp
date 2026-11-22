#include "employee_service.h"
#include "db_manager.h"
#include <codecvt>
#include <locale>
#include <sstream>
#include <iostream>
#include <cstring>
#include "db_utils.h"

// Внутренняя вспомогательная функция для конвертации строк
static std::wstring utf8_to_wstring(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    try { return myconv.from_bytes(str); } catch (...) { return L""; }
}

static std::wstring getWStringFromBuffer(unsigned char* buffer, unsigned int offset, short nullOffset) {
    if (*reinterpret_cast<short*>(buffer + nullOffset) == -1) return L"";
    short len = *reinterpret_cast<short*>(buffer + offset);
    char* text = reinterpret_cast<char*>(buffer + offset + 2);
    return utf8_to_wstring(std::string(text, len));
}

static std::string getStringFromBuffer(unsigned char* buffer, unsigned int offset, short nullOffset) {
    if (*reinterpret_cast<short*>(buffer + nullOffset) == -1) return "";
    short len = *reinterpret_cast<short*>(buffer + offset);
    char* text = reinterpret_cast<char*>(buffer + offset + 2);
    return std::string(text, len);
}

std::vector<Employee> get_employees_list() {
    std::vector<Employee> list;
    Firebird::IAttachment* att = DBManager::getAttachment();
    Firebird::IStatus* status = DBManager::getStatus();
    if (!att || !status) return list;

    Firebird::ThrowStatusWrapper wrappedStatus(status);
    Firebird::ITransaction* tra = nullptr;
    Firebird::IStatement* stmt = nullptr; // Используем Statement для Prepare
    Firebird::IResultSet* rs = nullptr;

    try {
        tra = att->startTransaction(&wrappedStatus, 0, nullptr);
        
        const char* sql = "SELECT ID, SN, SN_INT, FIRST_NAME, MIDDLE_NAME, LAST_NAME, PHOTO FROM employees ORDER BY ID;";
        
        // Компилируем запрос на сервере и выравниваем метаданные под С++
        stmt = att->prepare(&wrappedStatus, tra, 0, sql, 3, Firebird::IStatement::PREPARE_PREFETCH_METADATA);
        Firebird::IMessageMetadata* meta = stmt->getOutputMetadata(&wrappedStatus);
        Firebird::IMetadataBuilder* builder = meta->getBuilder(&wrappedStatus);
        Firebird::IMessageMetadata* alignedMeta = builder->getMetadata(&wrappedStatus);

        std::vector<unsigned char> buffer(alignedMeta->getMessageLength(&wrappedStatus));

        // Вычисляем точные смещения полей
        unsigned int offId = alignedMeta->getOffset(&wrappedStatus, 0);
        unsigned int offSn = alignedMeta->getOffset(&wrappedStatus, 1);
        unsigned int offSi = alignedMeta->getOffset(&wrappedStatus, 2);
        unsigned int offFn = alignedMeta->getOffset(&wrappedStatus, 3);
        unsigned int offMn = alignedMeta->getOffset(&wrappedStatus, 4);
        unsigned int offLn = alignedMeta->getOffset(&wrappedStatus, 5);
        short nullOffPhoto = alignedMeta->getNullOffset(&wrappedStatus, 6);

        short nullOffId = alignedMeta->getNullOffset(&wrappedStatus, 0);
        short nullOffSn = alignedMeta->getNullOffset(&wrappedStatus, 1);
        short nullOffSi = alignedMeta->getNullOffset(&wrappedStatus, 2);

        // Открываем курсор с 7 параметрами
        rs = stmt->openCursor(&wrappedStatus, tra, nullptr, nullptr, alignedMeta, 0);

        // Стабильный цикл чтения
        while (rs->fetchNext(&wrappedStatus, buffer.data()) == Firebird::IStatus::RESULT_OK) {
            Employee emp;
            
            emp.id = (*reinterpret_cast<short*>(buffer.data() + nullOffId) != -1) ? *reinterpret_cast<int*>(buffer.data() + offId) : 0;
            emp.sn = (*reinterpret_cast<short*>(buffer.data() + nullOffSn) != -1) ? *reinterpret_cast<long long*>(buffer.data() + offSn) : 0;
            emp.si = (*reinterpret_cast<short*>(buffer.data() + nullOffSi) != -1) ? *reinterpret_cast<long long*>(buffer.data() + offSi) : 0;
            
            emp.firstName  = getStringFromBuffer(buffer.data(), offFn, alignedMeta->getNullOffset(&wrappedStatus, 3));
            emp.middleName = getStringFromBuffer(buffer.data(), offMn, alignedMeta->getNullOffset(&wrappedStatus, 4));
            emp.lastName   = getStringFromBuffer(buffer.data(), offLn, alignedMeta->getNullOffset(&wrappedStatus, 5));
            emp.hasPhoto   = (*reinterpret_cast<short*>(buffer.data() + nullOffPhoto) != -1);
            
            list.push_back(emp);
        }

        // Порядок очистки строго снизу вверх
        alignedMeta->release();
        builder->release();
        meta->release();
        rs->close(&wrappedStatus); 
        stmt->free(&wrappedStatus); 
        tra->commit(&wrappedStatus);
        
    } catch (const Firebird::FbException& e) {
        // Каскадное закрытие дескрипторов на сервере
        if (rs) try { rs->close(&wrappedStatus); } catch(...) {}
        if (stmt) try { stmt->free(&wrappedStatus); } catch(...) {}
        if (tra) try { tra->rollback(&wrappedStatus); } catch(...) {}

        // --- ВЫВОД ОШИБКИ СУБД НА ЭКРАН ---
        // def_prog_mode(); // Сохраняем состояние TUI
        // endwin();        // Временно выключаем ncurses режим экрана

        std::cerr << "\n==================================================\n";
        std::cerr << "[КРИТИЧЕСКАЯ ОШИБКА БАЗЫ ДАННЫХ FIREBIRD]\n";
        
        // Буфер для текстового сообщения от сервера
        char err_msg[1024] = {0};
        // Вытаскиваем детальное описание ошибки из статуса исключения
        DBManager::getMaster()->getUtilInterface()->formatStatus(err_msg, sizeof(err_msg), e.getStatus());
        
        std::cerr << "Описание:\n" << err_msg << "\n";
        std::cerr << "==================================================\n";
        std::cerr << "Нажмите ENTER, чтобы вернуться в приложение...";
        std::cin.get(); // Ожидаем реакции пользователя

        // reset_prog_mode(); // Восстанавливаем параметры терминала
        // refresh();         // Перерисовываем ncurses интерфейс

    } catch (const std::exception& e) {
        if (rs) try { rs->close(&wrappedStatus); } catch(...) {}
        if (stmt) try { stmt->free(&wrappedStatus); } catch(...) {}
        if (tra) try { tra->rollback(&wrappedStatus); } catch(...) {}

        // --- ВЫВОД СИСТЕМНОЙ ОШИБКИ С++ ---
        // def_prog_mode();
        // endwin();

        std::cerr << "\n==================================================\n";
        std::cerr << "[СИСТЕМНАЯ ОШИБКА ПРИЛОЖЕНИЯ (C++)]\n";
        std::cerr << "Что пошло не так: " << e.what() << "\n";
        std::cerr << "==================================================\n";
        std::cerr << "Нажмите ENTER, чтобы вернуться в приложение...";
        std::cin.get();

        // reset_prog_mode();
        // refresh();

    } catch (...) {
        if (rs) try { rs->close(&wrappedStatus); } catch(...) {}
        if (stmt) try { stmt->free(&wrappedStatus); } catch(...) {}
        if (tra) try { tra->rollback(&wrappedStatus); } catch(...) {}

        // --- ВЫВОД НЕИЗВЕСТНОЙ ОШИБКИ ---
        // def_prog_mode();
        // endwin();

        std::cerr << "\n[ОШИБКА] Произошел неизвестный аппаратный сбой в памяти.\n";
        std::cerr << "Нажмите ENTER, чтобы вернуться в приложение...";
        std::cin.get();

        // reset_prog_mode();
        // refresh();
    }

    return list;
}

// РЕАЛИЗАЦИЯ: Полные данные сотрудника (с фото) для карточки
Employee get_employee_details(int employee_id) {
    Employee emp{};
    Firebird::IAttachment* att = DBManager::getAttachment();
    Firebird::IStatus* status = DBManager::getStatus();
    if (!att || !status) return emp;

    // ИСПРАВЛЕНИЕ: Оборачиваем статус
    Firebird::ThrowStatusWrapper wrappedStatus(status);

    Firebird::ITransaction* tra = nullptr;
    Firebird::IResultSet* rs = nullptr;

    try {
        tra = att->startTransaction(&wrappedStatus, 0, nullptr);
        std::stringstream sql_ss;
        sql_ss << "SELECT ID, SN, SN_INT, FIRST_NAME, MIDDLE_NAME, LAST_NAME, PHOTO FROM employees WHERE ID = " << employee_id << ";";
        
        rs = att->openCursor(&wrappedStatus, tra, 0, sql_ss.str().c_str(), 3, nullptr, nullptr, nullptr, nullptr, 0);
        Firebird::IMessageMetadata* meta = rs->getMetadata(&wrappedStatus);
        std::vector<unsigned char> buffer(meta->getMessageLength(&wrappedStatus));

        if (rs->fetchNext(&wrappedStatus, buffer.data()) == Firebird::IStatus::RESULT_OK) {
            emp.id = employee_id;
            emp.sn = (*reinterpret_cast<short*>(buffer.data() + meta->getNullOffset(&wrappedStatus, 1)) != -1) ? *reinterpret_cast<long long*>(buffer.data() + meta->getOffset(&wrappedStatus, 1)) : 0;
            emp.si = (*reinterpret_cast<short*>(buffer.data() + meta->getNullOffset(&wrappedStatus, 2)) != -1) ? *reinterpret_cast<long long*>(buffer.data() + meta->getOffset(&wrappedStatus, 2)) : 0;
            emp.firstName = getStringFromBuffer(buffer.data(), meta->getOffset(&wrappedStatus, 3), meta->getNullOffset(&wrappedStatus, 3));
            emp.middleName = getStringFromBuffer(buffer.data(), meta->getOffset(&wrappedStatus, 4), meta->getNullOffset(&wrappedStatus, 4));
            emp.lastName = getStringFromBuffer(buffer.data(), meta->getOffset(&wrappedStatus, 5), meta->getNullOffset(&wrappedStatus, 5));
            
            short nullOffPhoto = meta->getNullOffset(&wrappedStatus, 6);
            if (*reinterpret_cast<short*>(buffer.data() + nullOffPhoto) != -1) {
                emp.hasPhoto = true;
                
                // ИСПРАВЛЕНИЕ: Убран префикс Firebird:: перед ISC_QUAD
                ISC_QUAD* blobId = reinterpret_cast<ISC_QUAD*>(buffer.data() + meta->getOffset(&wrappedStatus, 6));
                Firebird::IBlob* blob = att->openBlob(&wrappedStatus, tra, blobId, 0, nullptr);
                
                std::vector<uint8_t> segmentBuffer(32767);
                unsigned int bytesRead = 0;
                int fetchResult;
                while ((fetchResult = blob->getSegment(&wrappedStatus, segmentBuffer.size(), segmentBuffer.data(), &bytesRead)) == Firebird::IStatus::RESULT_OK || status->getState() == 0) {
                    if (bytesRead > 0) emp.photo.insert(emp.photo.end(), segmentBuffer.begin(), segmentBuffer.begin() + bytesRead);
                    if (fetchResult != Firebird::IStatus::RESULT_OK) break;
                }
                blob->close(&wrappedStatus);
            }
        }
        meta->release(); 
        rs->close(&wrappedStatus); 
        tra->commit(&wrappedStatus);
    } catch (...) { 
        if (tra) tra->rollback(&wrappedStatus); 
    }
    return emp;
}

Result update_employee_name(
    int id,
    const std::string& last_name,
    const std::string& first_name,
    const std::string& middle_name) {
    auto attachment = DBManager::getAttachment();
    auto master = DBManager::getMaster();

    if (!attachment || !master) return {false, "No connect"};

    // Firebird::ThrowStatusWrapper status(master->getStatus());
    // Firebird::ITransaction* tra = nullptr;
    // Firebird::IStatement* stmt = nullptr;
    // Firebird::IMessageMetadata* meta = nullptr;
    FbStatus status(master);

    try {
        // tra = attachment->startTransaction(&status, 0, nullptr);
        // Транзакция сама сделает rollback и release, если вылетит исключение до commit()
        FbTransactionGuard tra(attachment->startTransaction(status.get(), 0, nullptr), master);

        const char* sql =
            "UPDATE EMPLOYEES "
            "SET LAST_NAME = ?, FIRST_NAME = ?, MIDDLE_NAME = ? "
            "WHERE ID = ?";

        // stmt = attachment->prepare(&status, tra, 0, sql, 3, Firebird::IStatement::PREPARE_PREFETCH_METADATA);
        // meta = stmt->getInputMetadata(&status);

        // stmt и meta сами вызовут release() при выходе из блока try (даже при ошибке)
        FbPtr<Firebird::IStatement> stmt(
            attachment->prepare(status.get(), tra.get(), 0, sql, 3, Firebird::IStatement::PREPARE_PREFETCH_METADATA)
            );
        FbPtr<Firebird::IMessageMetadata> meta(stmt->getInputMetadata(status.get()));

        unsigned msgLen = meta->getMessageLength(status.get());
        std::vector<unsigned char> buffer(msgLen, 0);

        // Вспомогательная лямбда для записи VARCHAR-параметра в буфер Firebird
        auto set_varchar = [&](unsigned index, const std::string& val) {
            unsigned offset = meta->getOffset(status.get(), index);
            unsigned nullOffset = meta->getNullOffset(status.get(), index);
            unsigned fieldLen = meta->getLength(status.get(), index);

            *reinterpret_cast<short*>(buffer.data() + nullOffset) = 0; // NOT NULL
            short copyLen = static_cast<short>(std::min<size_t>(val.size(), fieldLen));
            *reinterpret_cast<short*>(buffer.data() + offset) = copyLen;
            std::memcpy(buffer.data() + offset + 2, val.data(), copyLen);
        };

        set_varchar(0, last_name);
        set_varchar(1, first_name);
        set_varchar(2, middle_name);

        // Записываем целочисленный ID (индекс 3)
        unsigned idOffset = meta->getOffset(status.get(), 3);
        unsigned idNullOffset = meta->getNullOffset(status.get(), 3);
        *reinterpret_cast<short*>(buffer.data() + idNullOffset) = 0;
        *reinterpret_cast<int*>(buffer.data() + idOffset) = id;

        // stmt->execute(&status, tra, meta, buffer.data(), nullptr, nullptr);
        stmt->execute(status.get(), tra.get(), meta.get(), buffer.data(), nullptr, nullptr);
        tra.commit(status.get());

        // meta->release();
        // stmt->release();
        // tra->release();
        return {true,"Ok"};

    } catch (const Firebird::FbException& e) {
        // Извлекаем человекочитаемое сообщение об ошибке из статуса Firebird
        char err_buf[512];
        master->getUtilInterface()->formatStatus(err_buf, sizeof(err_buf), e.getStatus());

        // if (tra) {
        //     try {
        //         Firebird::ThrowStatusWrapper rollbackStatus(master->getStatus());
        //         tra->rollback(&rollbackStatus);
        //     } catch (...) {}
        //     tra->release();
        // }
        // if (meta) meta->release();
        // if (stmt) stmt->release();

        return {false,err_buf};
    }
}

Result update_employee_photo(int id, const std::vector<uint8_t>& photo_bytes) {
    auto attachment = DBManager::getAttachment();
    auto status_raw = DBManager::getStatus();
    auto master = DBManager::getMaster();

    if (!attachment || !status_raw || !master) return {false, "No connect"};

    Firebird::ThrowStatusWrapper status(status_raw);

    try {
        FbTransactionGuard tra(attachment->startTransaction(&status, 0, nullptr), master);

        // 1. Создаём новый BLOB и записываем в него байты изображения
        ISC_QUAD blob_id{};
        FbPtr<Firebird::IBlob> blob(
            attachment->createBlob(&status, tra.get(), &blob_id, 0, nullptr));

        size_t offset = 0;
        const size_t max_chunk = 32767;
        while (offset < photo_bytes.size()) {
            unsigned int chunk = static_cast<unsigned int>(
                std::min(max_chunk, photo_bytes.size() - offset));
            blob->putSegment(&status, chunk, photo_bytes.data() + offset);
            offset += chunk;
        }
        blob->close(&status);
        blob.release(); // close() уже освобождает интерфейс IBlob в Firebird OO API

        // 2. Привязываем созданный blob_id к записи сотрудника
        const char* sql = "UPDATE EMPLOYEES SET PHOTO = ? WHERE ID = ?";
        FbPtr<Firebird::IStatement> stmt(
            attachment->prepare(&status, tra.get(), 0, sql, 3, Firebird::IStatement::PREPARE_PREFETCH_METADATA));
        FbPtr<Firebird::IMessageMetadata> meta(stmt->getInputMetadata(&status));

        std::vector<unsigned char> buffer(meta->getMessageLength(&status), 0);

        unsigned blobOff = meta->getOffset(&status, 0);
        unsigned blobNullOff = meta->getNullOffset(&status, 0);
        *reinterpret_cast<short*>(buffer.data() + blobNullOff) = 0;
        *reinterpret_cast<ISC_QUAD*>(buffer.data() + blobOff) = blob_id;

        unsigned idOff = meta->getOffset(&status, 1);
        unsigned idNullOff = meta->getNullOffset(&status, 1);
        *reinterpret_cast<short*>(buffer.data() + idNullOff) = 0;
        *reinterpret_cast<int*>(buffer.data() + idOff) = id;

        stmt->execute(&status, tra.get(), meta.get(), buffer.data(), nullptr, nullptr);
        tra.commit(&status);

        return {true, "Ok"};
    } catch (const Firebird::FbException& e) {
        char err_buf[512] = {0};
        master->getUtilInterface()->formatStatus(err_buf, sizeof(err_buf), e.getStatus());
        return {false, err_buf};
    }
}
