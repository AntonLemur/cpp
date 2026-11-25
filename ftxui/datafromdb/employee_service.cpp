#include "employee_service.h"
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

Result<int> update_employee_name(
    int id,
    const std::string& last_name,
    const std::string& first_name,
    const std::string& middle_name) {
    auto attachment = DBManager::getAttachment();
    auto master = DBManager::getMaster();

    if (!attachment || !master) return Result<int>{false, "No connect", 0};

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
        return Result<int>{true, "Ok", 0};

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

        return Result<int>{false, err_buf, 0};
    }
}

Result<int> update_employee_photo(int id, const std::vector<uint8_t>& photo_bytes) {
    auto attachment = DBManager::getAttachment();
    auto status_raw = DBManager::getStatus();
    auto master = DBManager::getMaster();

    if (!attachment || !status_raw || !master) return Result<int>{false, "No connect", 0};

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

        return Result<int>{true, "Ok", 0};
    } catch (const Firebird::FbException& e) {
        char err_buf[512] = {0};
        master->getUtilInterface()->formatStatus(err_buf, sizeof(err_buf), e.getStatus());
        return Result<int>{false, err_buf, 0};
    }
}

// Входной параметр для SELECT ... WHERE ID = ?
FB_MESSAGE(EmployeeIdIn, Firebird::ThrowStatusWrapper,
           (FB_INTEGER, id)
);

// Выходные колонки запроса SELECT для полной карточки сотрудника
FB_MESSAGE(EmployeeCardOut, Firebird::ThrowStatusWrapper,
           (FB_INTEGER, id)
           (FB_BIGINT, sn)
           (FB_INTEGER, sn_int)          // <-- INTEGER Nullable -> FB_INTEGER (не FB_BIGINT!)
           (FB_VARCHAR(256), first_name)
           (FB_VARCHAR(256), middle_name)
           (FB_VARCHAR(256), last_name)
           (FB_VARCHAR(128), car_number)
           (FB_SMALLINT, permissions)
           (FB_TIME, work_begin)
           (FB_TIME, work_end)
           (FB_INTEGER, allowed_work_5p)
           (FB_SMALLINT, category)
           (FB_BLOB, photo) // <-- Поле BLOB (внутри содержит ISC_QUAD)
);

Result<Employee> get_employee_details_new(int employee_id) {
    Employee emp{};
    Firebird::IMaster* master = DBManager::getMaster();
    Firebird::IAttachment* att = DBManager::getAttachment();

    FbStatus status(master);

    if (!att || !status.get()) return Result<Employee>{false, "No connect", emp};

    FbTransactionGuard tra(att->startTransaction(status.get(), 0, nullptr), master);
    Firebird::IResultSet* rs = nullptr;

    try {
        FbTransactionGuard tra(att->startTransaction(status.get(), 0, nullptr), master);

        EmployeeIdIn in(status.get(), master);
        EmployeeCardOut out(status.get(), master);
        in.clear();
        out.clear();
        in->id = employee_id;

        const char* sql =
            "SELECT ID, SN, SN_INT, FIRST_NAME, MIDDLE_NAME, LAST_NAME, "
            "CAR_NUMBER, PERMISSIONS, WORK_BEGIN, WORK_END, "
            "ALLOWED_WORK_5P, CATEGORY, PHOTO "
            "FROM EMPLOYEES WHERE ID = ?";

        rs = att->openCursor(status.get(), tra.get(), 0, sql, SQL_DIALECT_V6,
                             in.getMetadata(), in.getData(),
                             out.getMetadata(), nullptr, 0);

        if (rs->fetchNext(status.get(), out.getData()) == Firebird::IStatus::RESULT_OK) {
            emp.id = out->id;
            if (!out->snNull) emp.sn = out->sn;
            if (!out->sn_intNull) emp.si = out->sn_int;

            // Чтение строк из FbVarChar: в выходном буфере out->first_name.str не заканчивается нулем (\0),
            // поэтому всегда используйте конструктор с явной длиной std::string(out->first_name.str, out->first_name.length).
            if (!out->first_nameNull)
                emp.firstName.assign(out->first_name.str, out->first_name.length);
            if (!out->middle_nameNull)
                emp.middleName.assign(out->middle_name.str, out->middle_name.length);
            if (!out->last_nameNull)
                emp.lastName.assign(out->last_name.str, out->last_name.length);

            if (!out->car_numberNull)
                emp.carNumber = std::string(out->car_number.str, out->car_number.length);
            if (!out->permissionsNull)
                emp.permissions = out->permissions;
            if (!out->work_beginNull)
                emp.workBegin = out->work_begin.value;
            if (!out->work_endNull)
                emp.workEnd = out->work_end.value;
            if (!out->allowed_work_5pNull)
                emp.allowedWork5p = out->allowed_work_5p;
            if (!out->categoryNull)
                emp.category = out->category;

            // Читаем BLOB (PHOTO), если он не NULL
            if (!out->photoNull) {


                // ISC_QUAD* blobId = reinterpret_cast<ISC_QUAD*>(buffer.data() + meta->getOffset(&wrappedStatus, 6));
                // Firebird::IBlob* blob = att->openBlob(&wrappedStatus, tra, blobId, 0, nullptr);

                // std::vector<uint8_t> segmentBuffer(32767);
                // unsigned int bytesRead = 0;
                // int fetchResult;
                // while ((fetchResult = blob->getSegment(&wrappedStatus, segmentBuffer.size(), segmentBuffer.data(), &bytesRead)) == Firebird::IStatus::RESULT_OK || status->getState() == 0) {
                //     if (bytesRead > 0) emp.photo.insert(emp.photo.end(), segmentBuffer.begin(), segmentBuffer.begin() + bytesRead);
                //     if (fetchResult != Firebird::IStatus::RESULT_OK) break;
                // }
                // blob->close(&wrappedStatus);


                emp.hasPhoto = true;
                FbPtr<Firebird::IBlob> blob(
                    att->openBlob(status.get(), tra.get(), &out->photo, 0, nullptr));
                try {
                    uint8_t chunk[32767];
                    unsigned int bytesRead = 0;
                    while (true) {
                        int res = blob->getSegment(status.get(), sizeof(chunk), chunk, &bytesRead);
                        if (res == Firebird::IStatus::RESULT_OK || res == Firebird::IStatus::RESULT_SEGMENT) {
                            emp.photo.insert(emp.photo.end(), chunk, chunk + bytesRead);
                        } else {
                            break; // RESULT_NO_DATA — достигнут конец BLOB
                        }
                    }
                    blob->close(status.get());
                } catch (const Firebird::FbException& e) {
                    char err_buf[512] = {0};
                    master->getUtilInterface()->formatStatus(err_buf, sizeof(err_buf), e.getStatus());
                    return Result<Employee>{false, err_buf, emp};
                }
            }
        }

        rs->close(status.get());
        rs = nullptr;
        tra.commit(status.get());
    } catch (const Firebird::FbException& e) {
        if (rs) rs->release();
        char err_buf[512] = {0};
        master->getUtilInterface()->formatStatus(err_buf, sizeof(err_buf), e.getStatus());
        return Result<Employee>{false, err_buf, emp};
    }

    return Result<Employee>{true, "Ok", emp};
}

// Описываем входные параметры строго в порядке их объявления в процедуре
FB_MESSAGE(EmployeeIn, Firebird::ThrowStatusWrapper,
           (FB_INTEGER, id)
           (FB_BIGINT, sn)
           (FB_VARCHAR(256), first_name)     // Лучше сразу 256 (64 * 4 байта для UTF-8)
           (FB_VARCHAR(256), middle_name)
           (FB_VARCHAR(256), last_name)
           (FB_VARCHAR(128), car_number)     // 32 * 4 байта
           (FB_SMALLINT, permissions)
           (FB_TIME, work_begin)
           (FB_TIME, work_end)
           (FB_INTEGER, allowed_work_5p)
           (FB_SMALLINT, category)
);

// Описываем выходной параметр
FB_MESSAGE(EmployeeOut, Firebird::ThrowStatusWrapper,
           (FB_INTEGER, r_id)
);

Result<int> update_employee(const Employee& emp) {

    auto att = DBManager::getAttachment();
    auto master = DBManager::getMaster();

    if (!att || !master) return Result<int>{false, "No connect", 0};

    FbStatus status(master);
    // IUtil* util = master->getUtilInterface();

    try {
        // tra = attachment->startTransaction(&status, 0, nullptr);
        // Транзакция сама сделает rollback и release, если вылетит исключение до commit()
        FbTransactionGuard tra(att->startTransaction(status.get(), 0, nullptr), master);

        // Создаем экземпляры сообщений (они сами выделяют буфер и строят метаданные)
        EmployeeIn in(status.get(), master);
        EmployeeOut out(status.get(), master);

        // Очищаем буферы (обнуляет флаги NULL)
        in.clear();
        out.clear();

        // ID (0 = INSERT, >0 = UPDATE)
        in->id = emp.id;

        // SN (BIGINT)
        if (emp.sn.has_value()) {
            in->sn = *emp.sn;
            in->snNull = FB_FALSE;
        } else {
            in->snNull = FB_TRUE;
        }

        // ФИО
        in->first_name.set(emp.firstName.c_str());
        in->middle_name.set(emp.middleName.c_str());
        in->last_name.set(emp.lastName.c_str());

        // Номер машины
        if (emp.carNumber.has_value()) {
            in->car_number.set(emp.carNumber->c_str());
            in->car_numberNull = FB_FALSE;
        } else {
            in->car_numberNull = FB_TRUE;
        }

        // Права (SMALLINT)
        if (emp.permissions.has_value()) {
            in->permissions = *emp.permissions;
            in->permissionsNull = FB_FALSE;
        } else {
            in->permissionsNull = FB_TRUE;
        }

        // Время начала и конца смены (TIME)
        if (emp.workBegin.has_value()) {
            in->work_begin.value = *emp.workBegin;
            in->work_beginNull = FB_FALSE;
        } else {
            in->work_beginNull = FB_TRUE;
        }

        if (emp.workEnd.has_value()) {
            in->work_end.value = *emp.workEnd;
            in->work_endNull = FB_FALSE;
        } else {
            in->work_endNull = FB_TRUE;
        }

        // Разрешение работы 5п
        in->allowed_work_5p = emp.allowedWork5p;

        // Категория
        if (emp.category.has_value()) {
            in->category = *emp.category;
            in->categoryNull = FB_FALSE;
        } else {
            in->categoryNull = FB_TRUE;
        }

        // Выполняем процедуру (11 входных параметров)
        att->execute(status.get(), tra.get(), 0,
            "EXECUTE PROCEDURE M_EMPLOYEES_IU(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
            SQL_DIALECT_V6,
            in.getMetadata(), in.getData(),
            out.getMetadata(), out.getData());
        tra.commit(status.get());
        if (out->r_idNull == FB_TRUE) { // или просто if (out->r_idNull)
            // Процедура вернула NULL
            return Result<int>{false, "The M_EMPLOYEES_IU procedure returned NULL", 0};
        } else {
            int newId = out->r_id;
            return Result<int>{true, "Ok", newId};
        }
    } catch (const Firebird::FbException& e) {
        // Извлекаем человекочитаемое сообщение об ошибке из статуса Firebird
        char err_buf[512];
        master->getUtilInterface()->formatStatus(err_buf, sizeof(err_buf), e.getStatus());
        return Result<int>{false, err_buf, 0};
    }
}
