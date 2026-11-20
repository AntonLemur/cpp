#ifndef DB_UTILS_H
#define DB_UTILS_H

#include <memory>

// В Firebird OO API все интерфейсы наследуются от IReferenceCounted (или IDisposable), у которых есть метод release().
// Чтобы не писать release() вручную в каждом try и catch, достаточно сделать небольшой шаблонный псевдоним поверх std::unique_ptr.

// Функтор-делитель для любых интерфейсов Firebird с методом release()
struct FbReleaser {
    template <typename T>
    void operator()(T* ptr) const noexcept {
        if (ptr) {
            ptr->release();
        }
    }
};

// Удобный шаблонный псевдоним
template <typename T>
using FbPtr = std::unique_ptr<T, FbReleaser>;

//RAII-обёртка для транзакции, которая автоматически делает rollback(), если не был вызван commit():
class FbTransactionGuard {
public:
    FbTransactionGuard(Firebird::ITransaction* tra, Firebird::IMaster* master)
        : tra_(tra), master_(master) {}

    ~FbTransactionGuard() {
        if (tra_ && !committed_) {
            try {
                Firebird::ThrowStatusWrapper status(master_->getStatus());
                tra_->rollback(&status);
            } catch (...) {}
        }
    }

    void commit(Firebird::ThrowStatusWrapper* status) {
        tra_->commit(status);
        committed_ = true;
    }

    Firebird::ITransaction* get() const noexcept { return tra_.get(); }

private:
    FbPtr<Firebird::ITransaction> tra_;
    Firebird::IMaster* master_;
    bool committed_ = false;
};

// Поскольку у IStatus вместо release() используется метод dispose(), для него пишется такой же крошечный делетер и удобный класс-обёртка:
struct FbDisposer {
    template <typename T>
    void operator()(T* ptr) const noexcept {
        if (ptr) {
            ptr->dispose();
        }
    }
};

template <typename T>
using FbDisposablePtr = std::unique_ptr<T, FbDisposer>;

// Готовая RAII-обёртка для статуса с автоматическим dispose()
class FbStatus {
public:
    explicit FbStatus(Firebird::IMaster* master)
        : raw_(master->getStatus()), wrapper_(raw_.get()) {}

    Firebird::ThrowStatusWrapper* get() noexcept { return &wrapper_; }
    Firebird::ThrowStatusWrapper* operator->() noexcept { return &wrapper_; }

private:
    FbDisposablePtr<Firebird::IStatus> raw_;
    Firebird::ThrowStatusWrapper wrapper_;
};

#endif // DB_UTILS_H
