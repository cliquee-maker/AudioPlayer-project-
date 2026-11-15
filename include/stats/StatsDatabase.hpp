#pragma once

// ============================================================================
// StatsDatabase — тонкая обёртка над QSqlDatabase для статистики прослушиваний
// и кэшей (тексты песен, ответы API). Без бизнес-логики: открытие/создание
// файла, PRAGMA, версионирование схемы через PRAGMA user_version, миграции.
//
// Каждый экземпляр получает своё имя соединения: SQLite-соединения Qt нельзя
// использовать из разных потоков, а имя по умолчанию конфликтует между
// экземплярами и тестами.
// ============================================================================

#include <QSqlDatabase>
#include <QString>

#include <filesystem>

// Путь <-> TEXT в БД. Единое поведение на Linux и Windows: UTF-8 и кириллица
// не зависят от текущей локали.
QString pathToDb(const std::filesystem::path& path);
std::filesystem::path pathFromDb(const QString& value);

class StatsDatabase {
public:
    // file — путь к файлу SQLite или спецзначение ":memory:" для тестов.
    explicit StatsDatabase(std::filesystem::path file);
    ~StatsDatabase();

    StatsDatabase(const StatsDatabase&) = delete;
    StatsDatabase& operator=(const StatsDatabase&) = delete;

    // Открывает (при необходимости создаёт) БД и применяет PRAGMA.
    // Битая БД сохраняется как stats.db.corrupt-<timestamp>, после чего
    // создаётся пустая и сразу мигрируется до актуальной версии.
    bool open(QString* error = nullptr);

    // Идемпотентно доводит схему до актуальной версии. Все шаги — в транзакции.
    bool migrate(QString* error = nullptr);

    bool isOpen() const;
    void close();

    QSqlDatabase db() const;

    // Версия схемы из PRAGMA user_version (-1, если БД не открыта).
    int schemaVersion() const;

    const std::filesystem::path& file() const;

private:
    bool openConnection(QString* error);
    bool applyPragmas(QString* error);
    bool probe(QString* error);
    bool backupBeforeMigration(QString* error);
    bool recoverFromCorruption(QString* error);
    bool isCorruption(const QString& errorText) const;
    bool isMemory() const;

    QString dbFileName() const;

    QString connectionName_;
    std::filesystem::path file_;
    bool recovering_ = false;
};
